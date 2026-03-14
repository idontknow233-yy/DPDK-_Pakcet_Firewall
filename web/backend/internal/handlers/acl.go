package handlers

import (
	"encoding/json"
	"errors"
	"fmt"
	"net/http"
	"strconv"
	"strings"

	"dpdk-packet-firewall-web-backend/internal/cli"
	"dpdk-packet-firewall-web-backend/internal/store"
)

type Handler struct {
	cli interface {
		Run(string) ([]string, error)
	}
	store *store.Store
}

func New(cliAddr string, st *store.Store) *Handler {
	return &Handler{
		cli:   &cli.Client{Addr: cliAddr},
		store: st,
	}
}

func (h *Handler) SyncToDpdk() error {
	if h == nil || h.cli == nil || h.store == nil {
		return errors.New("not ready")
	}
	desired4, err := h.store.ListByFamily(false)
	if err != nil {
		return err
	}
	desired6, err := h.store.ListByFamily(true)
	if err != nil {
		return err
	}
	if err := h.syncFamily(false, desired4); err != nil {
		return err
	}
	return h.syncFamily(true, desired6)
}

func (h *Handler) syncFamily(v6 bool, desired []store.Rule) error {
	listCmd := "acl list"
	clearCmd := "acl clear"
	delPrefix := "acl del "
	addAck := "ACL rule added"
	delAck := "ACL rule deleted"
	clearAck := "ACL cleared"
	parse := cli.ParseAclList
	addCmd := ruleToAddCmd
	if v6 {
		listCmd = "acl6 list"
		clearCmd = "acl6 clear"
		delPrefix = "acl6 del "
		addAck = "ACL6 rule added"
		delAck = "ACL6 rule deleted"
		clearAck = "ACL6 cleared"
		parse = cli.ParseAcl6List
		addCmd = ruleToAddCmd6
	}
	lines, err := h.cli.Run(listCmd)
	if err != nil {
		return h.syncFullReplayFamily(clearCmd, clearAck, addAck, desired, addCmd)
	}
	snap, err := parse(lines)
	if err != nil || snap == nil {
		return h.syncFullReplayFamily(clearCmd, clearAck, addAck, desired, addCmd)
	}
	current, err := snapshotToStoreRules(snap)
	if err != nil {
		return h.syncFullReplayFamily(clearCmd, clearAck, addAck, desired, addCmd)
	}
	return h.syncIncrementalFamily(current, desired, delPrefix, delAck, addCmd, addAck)
}

func (h *Handler) syncIncrementalFamily(current []store.Rule, desired []store.Rule, delPrefix string, delAck string, addCmd func(store.Rule) string, addAck string) error {
	prefix := 0
	for prefix < len(current) && prefix < len(desired) && equalRule(current[prefix], desired[prefix]) {
		prefix++
	}
	for i := len(current) - 1; i >= prefix; i-- {
		lines, err := h.cli.Run(delPrefix + strconv.Itoa(i))
		if err != nil {
			return err
		}
		if !contains(lines, delAck) {
			return errors.New("dpdk del failed")
		}
	}
	for i := prefix; i < len(desired); i++ {
		lines, err := h.cli.Run(addCmd(desired[i]))
		if err != nil {
			return err
		}
		if !contains(lines, addAck) {
			return errors.New("dpdk add failed")
		}
	}
	return nil
}

func (h *Handler) syncFullReplayFamily(clearCmd string, clearAck string, addAck string, rules []store.Rule, addCmd func(store.Rule) string) error {
	lines, err := h.cli.Run(clearCmd)
	if err != nil {
		return err
	}
	if !contains(lines, clearAck) {
		return errors.New("dpdk clear failed")
	}
	for _, r := range rules {
		lines, err := h.cli.Run(addCmd(r))
		if err != nil {
			return err
		}
		if !contains(lines, addAck) {
			return errors.New("dpdk add failed")
		}
	}
	return nil
}

func (h *Handler) Health(w http.ResponseWriter, r *http.Request) {
	_, err := h.cli.Run("acl list")
	if err != nil {
		http.Error(w, "unhealthy", http.StatusServiceUnavailable)
		return
	}
	_, err = h.cli.Run("acl6 list")
	if err != nil {
		http.Error(w, "unhealthy", http.StatusServiceUnavailable)
		return
	}
	w.WriteHeader(http.StatusOK)
}

func (h *Handler) Acl(w http.ResponseWriter, r *http.Request) {
	switch r.Method {
	case http.MethodGet:
		rules, err := h.store.ListByFamily(false)
		if err != nil {
			http.Error(w, "list failed", http.StatusInternalServerError)
			return
		}
		version := int64(0)
		if lines, err := h.cli.Run("acl list"); err == nil {
			if snap, _ := cli.ParseAclList(lines); snap != nil {
				version = snap.Version
			}
		}
		type OutRule struct {
			Index  int    `json:"index"`
			Action string `json:"action"`
			Src    string `json:"src"`
			Dst    string `json:"dst"`
			Proto  string `json:"proto"`
			Sport  string `json:"sport"`
			Dport  string `json:"dport"`
		}
		outRules := make([]OutRule, 0, len(rules))
		for i, rr := range rules {
			proto := "any"
			if rr.Proto != 0 {
				proto = strconv.Itoa(rr.Proto)
			}
			sport := "any"
			dport := "any"
			if rr.Proto != 0 || rr.SrcPortMax != 0 || rr.DstPortMax != 0 {
				sport = strconv.Itoa(rr.SrcPortMin) + "-" + strconv.Itoa(rr.SrcPortMax)
				dport = strconv.Itoa(rr.DstPortMin) + "-" + strconv.Itoa(rr.DstPortMax)
			}
			outRules = append(outRules, OutRule{
				Index: i, Action: rr.Action, Src: rr.Src, Dst: rr.Dst,
				Proto: proto, Sport: sport, Dport: dport,
			})
		}
		out := struct {
			Version int64     `json:"version"`
			Count   int       `json:"count"`
			Rules   []OutRule `json:"rules"`
		}{Version: version, Count: len(outRules), Rules: outRules}
		w.Header().Set("Content-Type", "application/json")
		_ = json.NewEncoder(w).Encode(out)
		return
	case http.MethodPost:
		var in struct {
			Action     string `json:"action"`
			Src        string `json:"src"`
			Dst        string `json:"dst"`
			Proto      int    `json:"proto"`
			SrcPortMin int    `json:"src_port_min"`
			SrcPortMax int    `json:"src_port_max"`
			DstPortMin int    `json:"dst_port_min"`
			DstPortMax int    `json:"dst_port_max"`
		}
		if json.NewDecoder(r.Body).Decode(&in) != nil {
			http.Error(w, "bad request", http.StatusBadRequest)
			return
		}
		if in.Src == "" {
			in.Src = "0.0.0.0/0"
		}
		if in.Dst == "" {
			in.Dst = "0.0.0.0/0"
		}
		act := strings.ToLower(in.Action)
		if err := h.store.Add(store.Rule{
			Action:     act,
			Src:        in.Src,
			Dst:        in.Dst,
			Proto:      in.Proto,
			SrcPortMin: in.SrcPortMin,
			SrcPortMax: in.SrcPortMax,
			DstPortMin: in.DstPortMin,
			DstPortMax: in.DstPortMax,
		}); err != nil {
			http.Error(w, "bad request", http.StatusBadRequest)
			return
		}
		if err := h.SyncToDpdk(); err != nil {
			http.Error(w, "apply failed", http.StatusBadGateway)
			return
		}
		w.WriteHeader(http.StatusOK)
		return
	case http.MethodDelete:
		if err := h.store.ClearByFamily(false); err != nil {
			http.Error(w, "clear failed", http.StatusInternalServerError)
			return
		}
		if err := h.SyncToDpdk(); err != nil {
			http.Error(w, "apply failed", http.StatusBadGateway)
			return
		}
		w.WriteHeader(http.StatusOK)
		return
	default:
		w.WriteHeader(http.StatusMethodNotAllowed)
		return
	}
}

func (h *Handler) AclItem(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodDelete {
		w.WriteHeader(http.StatusMethodNotAllowed)
		return
	}
	p := strings.TrimPrefix(r.URL.Path, "/api/acl/")
	p = strings.TrimSuffix(p, "/")
	if p == "" {
		h.Acl(w, r)
		return
	}
	idx, err := strconv.Atoi(p)
	if err != nil {
		http.Error(w, "bad index", http.StatusBadRequest)
		return
	}
	if err := h.store.DeleteByIndexByFamily(idx, false); err != nil {
		http.Error(w, "delete failed", http.StatusBadRequest)
		return
	}
	if err := h.SyncToDpdk(); err != nil {
		http.Error(w, "apply failed", http.StatusBadGateway)
		return
	}
	w.WriteHeader(http.StatusOK)
}

func (h *Handler) Acl6(w http.ResponseWriter, r *http.Request) {
	switch r.Method {
	case http.MethodGet:
		rules, err := h.store.ListByFamily(true)
		if err != nil {
			http.Error(w, "list failed", http.StatusInternalServerError)
			return
		}
		version := int64(0)
		if lines, err := h.cli.Run("acl6 list"); err == nil {
			if snap, _ := cli.ParseAcl6List(lines); snap != nil {
				version = snap.Version
			}
		}
		type OutRule struct {
			Index  int    `json:"index"`
			Action string `json:"action"`
			Src    string `json:"src"`
			Dst    string `json:"dst"`
			Proto  string `json:"proto"`
			Sport  string `json:"sport"`
			Dport  string `json:"dport"`
		}
		outRules := make([]OutRule, 0, len(rules))
		for i, rr := range rules {
			proto := "any"
			if rr.Proto != 0 {
				proto = strconv.Itoa(rr.Proto)
			}
			sport := "any"
			dport := "any"
			if rr.Proto != 0 || rr.SrcPortMax != 0 || rr.DstPortMax != 0 {
				sport = strconv.Itoa(rr.SrcPortMin) + "-" + strconv.Itoa(rr.SrcPortMax)
				dport = strconv.Itoa(rr.DstPortMin) + "-" + strconv.Itoa(rr.DstPortMax)
			}
			outRules = append(outRules, OutRule{
				Index: i, Action: rr.Action, Src: rr.Src, Dst: rr.Dst,
				Proto: proto, Sport: sport, Dport: dport,
			})
		}
		out := struct {
			Version int64     `json:"version"`
			Count   int       `json:"count"`
			Rules   []OutRule `json:"rules"`
		}{Version: version, Count: len(outRules), Rules: outRules}
		w.Header().Set("Content-Type", "application/json")
		_ = json.NewEncoder(w).Encode(out)
		return
	case http.MethodPost:
		var in struct {
			Action     string `json:"action"`
			Src        string `json:"src"`
			Dst        string `json:"dst"`
			Proto      int    `json:"proto"`
			SrcPortMin int    `json:"src_port_min"`
			SrcPortMax int    `json:"src_port_max"`
			DstPortMin int    `json:"dst_port_min"`
			DstPortMax int    `json:"dst_port_max"`
		}
		if json.NewDecoder(r.Body).Decode(&in) != nil {
			http.Error(w, "bad request", http.StatusBadRequest)
			return
		}
		if in.Src == "" {
			in.Src = "::/0"
		}
		if in.Dst == "" {
			in.Dst = "::/0"
		}
		act := strings.ToLower(in.Action)
		if err := h.store.Add(store.Rule{
			Action:     act,
			Src:        in.Src,
			Dst:        in.Dst,
			Proto:      in.Proto,
			SrcPortMin: in.SrcPortMin,
			SrcPortMax: in.SrcPortMax,
			DstPortMin: in.DstPortMin,
			DstPortMax: in.DstPortMax,
		}); err != nil {
			http.Error(w, "bad request", http.StatusBadRequest)
			return
		}
		if err := h.SyncToDpdk(); err != nil {
			http.Error(w, "apply failed", http.StatusBadGateway)
			return
		}
		w.WriteHeader(http.StatusOK)
		return
	case http.MethodDelete:
		if err := h.store.ClearByFamily(true); err != nil {
			http.Error(w, "clear failed", http.StatusInternalServerError)
			return
		}
		if err := h.SyncToDpdk(); err != nil {
			http.Error(w, "apply failed", http.StatusBadGateway)
			return
		}
		w.WriteHeader(http.StatusOK)
		return
	default:
		w.WriteHeader(http.StatusMethodNotAllowed)
		return
	}
}

func (h *Handler) Acl6Item(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodDelete {
		w.WriteHeader(http.StatusMethodNotAllowed)
		return
	}
	p := strings.TrimPrefix(r.URL.Path, "/api/acl6/")
	p = strings.TrimSuffix(p, "/")
	if p == "" {
		h.Acl6(w, r)
		return
	}
	idx, err := strconv.Atoi(p)
	if err != nil {
		http.Error(w, "bad index", http.StatusBadRequest)
		return
	}
	if err := h.store.DeleteByIndexByFamily(idx, true); err != nil {
		http.Error(w, "delete failed", http.StatusBadRequest)
		return
	}
	if err := h.SyncToDpdk(); err != nil {
		http.Error(w, "apply failed", http.StatusBadGateway)
		return
	}
	w.WriteHeader(http.StatusOK)
}

func contains(lines []string, s string) bool {
	for _, l := range lines {
		if strings.Contains(l, s) {
			return true
		}
	}
	return false
}

func ruleToAddCmd(r store.Rule) string {
	return "acl add " + r.Action + " " + r.Src + " " + r.Dst + " " + strconv.Itoa(r.Proto) + " " +
		strconv.Itoa(r.SrcPortMin) + " " + strconv.Itoa(r.SrcPortMax) + " " +
		strconv.Itoa(r.DstPortMin) + " " + strconv.Itoa(r.DstPortMax)
}

func ruleToAddCmd6(r store.Rule) string {
	return "acl6 add " + r.Action + " " + r.Src + " " + r.Dst + " " + strconv.Itoa(r.Proto) + " " +
		strconv.Itoa(r.SrcPortMin) + " " + strconv.Itoa(r.SrcPortMax) + " " +
		strconv.Itoa(r.DstPortMin) + " " + strconv.Itoa(r.DstPortMax)
}

func equalRule(a store.Rule, b store.Rule) bool {
	return a.Action == b.Action &&
		a.Src == b.Src &&
		a.Dst == b.Dst &&
		a.Proto == b.Proto &&
		a.SrcPortMin == b.SrcPortMin &&
		a.SrcPortMax == b.SrcPortMax &&
		a.DstPortMin == b.DstPortMin &&
		a.DstPortMax == b.DstPortMax
}

func snapshotToStoreRules(snap *cli.Snapshot) ([]store.Rule, error) {
	out := make([]store.Rule, 0, len(snap.Rules))
	for _, r := range snap.Rules {
		proto, err := parseProto(r.Proto)
		if err != nil {
			return nil, err
		}
		spMin, spMax, err := parsePortRange(r.Sport)
		if err != nil {
			return nil, err
		}
		dpMin, dpMax, err := parsePortRange(r.Dport)
		if err != nil {
			return nil, err
		}
		out = append(out, store.Rule{
			Action:     r.Action,
			Src:        r.Src,
			Dst:        r.Dst,
			Proto:      proto,
			SrcPortMin: spMin,
			SrcPortMax: spMax,
			DstPortMin: dpMin,
			DstPortMax: dpMax,
		})
	}
	return out, nil
}

func parseProto(v string) (int, error) {
	if v == "any" {
		return 0, nil
	}
	n, err := strconv.Atoi(v)
	if err != nil {
		return 0, fmt.Errorf("bad proto: %s", v)
	}
	return n, nil
}

func parsePortRange(v string) (int, int, error) {
	if v == "any" {
		return 0, 0, nil
	}
	parts := strings.Split(v, "-")
	if len(parts) != 2 {
		return 0, 0, fmt.Errorf("bad port range: %s", v)
	}
	minV, err := strconv.Atoi(parts[0])
	if err != nil {
		return 0, 0, fmt.Errorf("bad port range: %s", v)
	}
	maxV, err := strconv.Atoi(parts[1])
	if err != nil {
		return 0, 0, fmt.Errorf("bad port range: %s", v)
	}
	return minV, maxV, nil
}
