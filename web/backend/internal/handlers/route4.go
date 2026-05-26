package handlers

import (
	"encoding/json"
	"net/http"
	"strconv"
	"strings"

	"dpdk-packet-firewall-web-backend/internal/cli"
)

func (h *Handler) Route4(w http.ResponseWriter, r *http.Request) {
	switch r.Method {
	case http.MethodGet:
		lines, err := h.cli.Run("route list")
		if err != nil {
			http.Error(w, "route list failed", http.StatusBadGateway)
			return
		}
		snap, _ := cli.ParseRoute4List(lines)
		if snap == nil {
			http.Error(w, "route parse failed", http.StatusBadGateway)
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
			nh = "0.0.0.0"
		}
		cmd := "route add " + strings.TrimSpace(in.Dst) + " " + nh + " " + strconv.Itoa(in.Port)
		lines, err := h.cli.Run(cmd)
		if err != nil || !contains(lines, "route add ok") {
			http.Error(w, "apply failed", http.StatusBadGateway)
			return
		}
		w.WriteHeader(http.StatusOK)
		return
	case http.MethodDelete:
		lines, err := h.cli.Run("route clear")
		if err != nil || !contains(lines, "route clear ok") {
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

func (h *Handler) Route4Item(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodDelete {
		w.WriteHeader(http.StatusMethodNotAllowed)
		return
	}
	p := strings.TrimPrefix(r.URL.Path, "/api/route4/")
	p = strings.TrimSuffix(p, "/")
	if p == "" {
		h.Route4(w, r)
		return
	}
	idx, err := strconv.Atoi(p)
	if err != nil || idx < 0 {
		http.Error(w, "bad index", http.StatusBadRequest)
		return
	}
	lines, err := h.cli.Run("route del " + strconv.Itoa(idx))
	if err != nil || !contains(lines, "route del ok") {
		http.Error(w, "apply failed", http.StatusBadGateway)
		return
	}
	w.WriteHeader(http.StatusOK)
}
