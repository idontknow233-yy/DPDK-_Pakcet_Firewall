package handlers

import (
	"encoding/json"
	"net/http"
	"strconv"

	"dpdk-packet-firewall-web-backend/internal/cli"
)

func (h *Handler) Sessions(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodGet {
		w.WriteHeader(http.StatusMethodNotAllowed)
		return
	}
	limit := 200
	if v := r.URL.Query().Get("limit"); v != "" {
		if n, err := strconv.Atoi(v); err == nil && n > 0 && n <= 4096 {
			limit = n
		}
	}
	lines, err := h.cli.Run("session list " + strconv.Itoa(limit))
	if err != nil {
		http.Error(w, "session list failed", http.StatusBadGateway)
		return
	}
	snap, _ := cli.ParseSessionList(lines)
	if snap == nil {
		http.Error(w, "session parse failed", http.StatusBadGateway)
		return
	}
	out := struct {
		Version  int64           `json:"version"`
		Count    int             `json:"count"`
		Sessions []cli.SessionRow `json:"sessions"`
	}{
		Version: snap.Version,
		Count:   snap.Count,
		Sessions: snap.Sessions,
	}
	w.Header().Set("Content-Type", "application/json")
	_ = json.NewEncoder(w).Encode(out)
}
