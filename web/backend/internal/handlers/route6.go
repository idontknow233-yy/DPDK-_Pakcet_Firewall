package handlers

import (
	"encoding/json"
	"net/http"
	"strconv"
	"strings"

	"dpdk-packet-firewall-web-backend/internal/cli"
)

func (h *Handler) Ifcfg6(w http.ResponseWriter, r *http.Request) {
	switch r.Method {
	case http.MethodGet:
		lines, err := h.cli.Run("ifcfg6 show")
		if err != nil {
			http.Error(w, "ifcfg6 show failed", http.StatusBadGateway)
			return
		}
		snap, _ := cli.ParseIfcfg6Show(lines)
		if snap == nil {
			http.Error(w, "ifcfg6 parse failed", http.StatusBadGateway)
			return
		}
		w.Header().Set("Content-Type", "application/json")
		_ = json.NewEncoder(w).Encode(snap)
		return
	case http.MethodPost:
		var in struct {
			Port int    `json:"port"`
			CIDR string `json:"cidr"`
		}
		if json.NewDecoder(r.Body).Decode(&in) != nil {
			http.Error(w, "bad request", http.StatusBadRequest)
			return
		}
		if in.Port < 0 {
			http.Error(w, "bad request", http.StatusBadRequest)
			return
		}
		cmd := "ifcfg6 set " + strconv.Itoa(in.Port) + " " + strings.TrimSpace(in.CIDR)
		lines, err := h.cli.Run(cmd)
		if err != nil || !contains(lines, "ifcfg6 set ok") {
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

func (h *Handler) Ifcfg6Item(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodDelete {
		w.WriteHeader(http.StatusMethodNotAllowed)
		return
	}
	p := strings.TrimPrefix(r.URL.Path, "/api/ifcfg6/")
	p = strings.TrimSuffix(p, "/")
	if p == "" {
		http.Error(w, "bad port", http.StatusBadRequest)
		return
	}
	port, err := strconv.Atoi(p)
	if err != nil || port < 0 {
		http.Error(w, "bad port", http.StatusBadRequest)
		return
	}
	lines, err := h.cli.Run("ifcfg6 clear " + strconv.Itoa(port))
	if err != nil || !contains(lines, "ifcfg6 clear ok") {
		http.Error(w, "apply failed", http.StatusBadGateway)
		return
	}
	w.WriteHeader(http.StatusOK)
}

func (h *Handler) Route6(w http.ResponseWriter, r *http.Request) {
	switch r.Method {
	case http.MethodGet:
		lines, err := h.cli.Run("route6 list")
		if err != nil {
			http.Error(w, "route6 list failed", http.StatusBadGateway)
			return
		}
		snap, _ := cli.ParseRoute6List(lines)
		if snap == nil {
			http.Error(w, "route6 parse failed", http.StatusBadGateway)
			return
		}
		w.Header().Set("Content-Type", "application/json")
		_ = json.NewEncoder(w).Encode(snap)
		return
	case http.MethodPost:
		var in struct {
			Dst  string `json:"dst"`
			Nh   string `json:"nh"`
			Port int    `json:"port"`
		}
		if json.NewDecoder(r.Body).Decode(&in) != nil {
			http.Error(w, "bad request", http.StatusBadRequest)
			return
		}
		if in.Port < 0 {
			http.Error(w, "bad request", http.StatusBadRequest)
			return
		}
		nh := strings.TrimSpace(in.Nh)
		if nh == "" {
			nh = "::"
		}
		cmd := "route6 add " + strings.TrimSpace(in.Dst) + " " + nh + " " + strconv.Itoa(in.Port)
		lines, err := h.cli.Run(cmd)
		if err != nil || !contains(lines, "route6 add ok") {
			http.Error(w, "apply failed", http.StatusBadGateway)
			return
		}
		w.WriteHeader(http.StatusOK)
		return
	case http.MethodDelete:
		lines, err := h.cli.Run("route6 clear")
		if err != nil || !contains(lines, "route6 clear ok") {
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

func (h *Handler) Route6Item(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodDelete {
		w.WriteHeader(http.StatusMethodNotAllowed)
		return
	}
	p := strings.TrimPrefix(r.URL.Path, "/api/route6/")
	p = strings.TrimSuffix(p, "/")
	if p == "" {
		h.Route6(w, r)
		return
	}
	idx, err := strconv.Atoi(p)
	if err != nil || idx < 0 {
		http.Error(w, "bad index", http.StatusBadRequest)
		return
	}
	lines, err := h.cli.Run("route6 del " + strconv.Itoa(idx))
	if err != nil || !contains(lines, "route6 del ok") {
		http.Error(w, "apply failed", http.StatusBadGateway)
		return
	}
	w.WriteHeader(http.StatusOK)
}

