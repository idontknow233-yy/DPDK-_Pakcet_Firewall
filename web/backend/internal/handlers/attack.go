package handlers

import (
	"encoding/json"
	"net/http"
	"strconv"

	"dpdk-packet-firewall-web-backend/internal/cli"
)

func (h *Handler) Attack(w http.ResponseWriter, r *http.Request) {
	switch r.Method {
	case http.MethodGet:
		lines, err := h.cli.Run("attack show")
		if err != nil {
			http.Error(w, "attack show failed", http.StatusBadGateway)
			return
		}
		snap, _ := cli.ParseAttackShow(lines)
		w.Header().Set("Content-Type", "application/json")
		_ = json.NewEncoder(w).Encode(snap)
		return
	case http.MethodPost:
		var in struct {
			Mitigation   int `json:"mitigation"`
			ScanPortsSec int `json:"scan_ports_sec"`
			BanSec       int `json:"ban_sec"`
		}
		if json.NewDecoder(r.Body).Decode(&in) != nil {
			http.Error(w, "bad request", http.StatusBadRequest)
			return
		}
		if in.Mitigation != 0 && in.Mitigation != 1 {
			http.Error(w, "bad request", http.StatusBadRequest)
			return
		}
		if in.ScanPortsSec <= 0 || in.BanSec <= 0 {
			http.Error(w, "bad request", http.StatusBadRequest)
			return
		}
		cmd := "attack set " + strconv.Itoa(in.Mitigation) + " " + strconv.Itoa(in.ScanPortsSec) + " " + strconv.Itoa(in.BanSec)
		lines, err := h.cli.Run(cmd)
		if err != nil || !contains(lines, "attack set ok") {
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

