package handlers

import (
	"encoding/json"
	"net/http"
	"strconv"
	"strings"

	"dpdk-packet-firewall-web-backend/internal/cli"
)

func (h *Handler) Ifcfg4(w http.ResponseWriter, r *http.Request) {
	switch r.Method {
	case http.MethodGet:
		lines, err := h.cli.Run("ifcfg4 show")
		if err != nil {
			http.Error(w, "ifcfg4 show failed", http.StatusBadGateway)
			return
		}
		snap, _ := cli.ParseIfcfg4Show(lines)
		if snap == nil {
			http.Error(w, "ifcfg4 parse failed", http.StatusBadGateway)
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
		cmd := "ifcfg4 set " + strconv.Itoa(in.Port) + " " + strings.TrimSpace(in.CIDR)
		lines, err := h.cli.Run(cmd)
		if err != nil || !contains(lines, "ifcfg4 set ok") {
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

func (h *Handler) Ifcfg4Item(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodDelete {
		w.WriteHeader(http.StatusMethodNotAllowed)
		return
	}
	p := strings.TrimPrefix(r.URL.Path, "/api/ifcfg4/")
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
	lines, err := h.cli.Run("ifcfg4 clear " + strconv.Itoa(port))
	if err != nil || !contains(lines, "ifcfg4 clear ok") {
		http.Error(w, "apply failed", http.StatusBadGateway)
		return
	}
	w.WriteHeader(http.StatusOK)
}
