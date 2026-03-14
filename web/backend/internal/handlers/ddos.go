package handlers

import (
	"encoding/json"
	"net/http"
	"strconv"

	"dpdk-packet-firewall-web-backend/internal/cli"
)

func (h *Handler) Ddos(w http.ResponseWriter, r *http.Request) {
	switch r.Method {
	case http.MethodGet:
		lines, err := h.cli.Run("ddos show")
		if err != nil {
			http.Error(w, "ddos show failed", http.StatusBadGateway)
			return
		}
		cfg, _ := cli.ParseDdosShow(lines)
		if cfg == nil {
			http.Error(w, "ddos parse failed", http.StatusBadGateway)
			return
		}
		w.Header().Set("Content-Type", "application/json")
		_ = json.NewEncoder(w).Encode(cfg)
		return
	case http.MethodPost:
		var in struct {
			SynPps   uint32 `json:"syn_pps"`
			SynBurst uint32 `json:"syn_burst"`
			UdpPps   uint32 `json:"udp_pps"`
			UdpBurst uint32 `json:"udp_burst"`
		}
		if json.NewDecoder(r.Body).Decode(&in) != nil {
			http.Error(w, "bad request", http.StatusBadRequest)
			return
		}
		cmd := "ddos set " + strconv.FormatUint(uint64(in.SynPps), 10) + " " + strconv.FormatUint(uint64(in.SynBurst), 10) + " " +
			strconv.FormatUint(uint64(in.UdpPps), 10) + " " + strconv.FormatUint(uint64(in.UdpBurst), 10)
		lines, err := h.cli.Run(cmd)
		if err != nil {
			http.Error(w, "ddos set failed", http.StatusBadGateway)
			return
		}
		ok := false
		for _, l := range lines {
			if l == "DDOS set ok" {
				ok = true
				break
			}
		}
		if !ok {
			http.Error(w, "ddos set failed", http.StatusBadGateway)
			return
		}
		w.WriteHeader(http.StatusOK)
		return
	default:
		w.WriteHeader(http.StatusMethodNotAllowed)
		return
	}
}

