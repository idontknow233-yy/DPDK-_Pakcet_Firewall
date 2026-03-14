package handlers

import (
	"encoding/json"
	"net/http"
	"strconv"

	"dpdk-packet-firewall-web-backend/internal/cli"
)

func (h *Handler) Denies6(w http.ResponseWriter, r *http.Request) {
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
	lines, err := h.cli.Run("deny6 list " + strconv.Itoa(limit))
	if err != nil {
		http.Error(w, "deny6 list failed", http.StatusBadGateway)
		return
	}
	snap, _ := cli.ParseDeny6List(lines)
	if snap == nil {
		http.Error(w, "deny6 list parse failed", http.StatusBadGateway)
		return
	}
	w.Header().Set("Content-Type", "application/json")
	_ = json.NewEncoder(w).Encode(snap)
}

