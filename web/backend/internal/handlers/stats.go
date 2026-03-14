package handlers

import (
	"encoding/json"
	"net/http"
	"strconv"

	"dpdk-packet-firewall-web-backend/internal/cli"
)

func (h *Handler) PortStats(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodGet {
		w.WriteHeader(http.StatusMethodNotAllowed)
		return
	}
	lines, err := h.cli.Run("port stats")
	if err != nil {
		http.Error(w, "port stats failed", http.StatusBadGateway)
		return
	}
	snap, _ := cli.ParsePortStats(lines)
	if snap == nil {
		http.Error(w, "port stats parse failed", http.StatusBadGateway)
		return
	}
	w.Header().Set("Content-Type", "application/json")
	_ = json.NewEncoder(w).Encode(snap)
}

func (h *Handler) Denies(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodGet {
		w.WriteHeader(http.StatusMethodNotAllowed)
		return
	}
	limit := 50
	if v := r.URL.Query().Get("limit"); v != "" {
		if n, err := strconv.Atoi(v); err == nil && n > 0 && n <= 2048 {
			limit = n
		}
	}
	lines, err := h.cli.Run("deny list " + strconv.Itoa(limit))
	if err != nil {
		http.Error(w, "deny list failed", http.StatusBadGateway)
		return
	}
	snap, _ := cli.ParseDenyList(lines)
	if snap == nil {
		http.Error(w, "deny list parse failed", http.StatusBadGateway)
		return
	}
	w.Header().Set("Content-Type", "application/json")
	_ = json.NewEncoder(w).Encode(snap)
}

