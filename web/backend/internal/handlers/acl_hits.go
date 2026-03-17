package handlers

import (
	"encoding/json"
	"net/http"

	"dpdk-packet-firewall-web-backend/internal/cli"
)

func (h *Handler) AclHits(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodGet {
		w.WriteHeader(http.StatusMethodNotAllowed)
		return
	}
	lines, err := h.cli.Run("acl hits")
	if err != nil {
		http.Error(w, "acl hits failed", http.StatusBadGateway)
		return
	}
	snap, _ := cli.ParseAclHits(lines)
	w.Header().Set("Content-Type", "application/json")
	_ = json.NewEncoder(w).Encode(snap)
}

func (h *Handler) Acl6Hits(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodGet {
		w.WriteHeader(http.StatusMethodNotAllowed)
		return
	}
	lines, err := h.cli.Run("acl6 hits")
	if err != nil {
		http.Error(w, "acl6 hits failed", http.StatusBadGateway)
		return
	}
	snap, _ := cli.ParseAcl6Hits(lines)
	w.Header().Set("Content-Type", "application/json")
	_ = json.NewEncoder(w).Encode(snap)
}

