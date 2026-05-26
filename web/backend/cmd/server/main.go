package main

import (
	"log"
	"net/http"
	"os"
	"time"

	"dpdk-packet-firewall-web-backend/internal/config"
	"dpdk-packet-firewall-web-backend/internal/handlers"
	"dpdk-packet-firewall-web-backend/internal/store"
)

func main() {
	addr := os.Getenv("HTTP_ADDR")
	if addr == "" {
		addr = ":9000"
	}

	dbPath := os.Getenv("RULE_DB")
	if dbPath == "" {
		dbPath = "./rules.db"
	}
	_, statErr := os.Stat(dbPath)
	dbExisted := statErr == nil

	st, err := store.Open(dbPath)
	if err != nil {
		log.Fatal(err)
	}
	defer st.Close()
	if !dbExisted && os.Getenv("SEED_DEFAULT_RULES") != "0" {
		_ = st.SeedDefaultsIfEmpty()
	}

	h := handlers.New(config.CLIAddr(), st)
	if err := h.SyncToDpdk(); err != nil {
		log.Println("sync to dpdk failed:", err)
		go func() {
			t := time.NewTicker(2 * time.Second)
			defer t.Stop()
			for range t.C {
				if err := h.SyncToDpdk(); err == nil {
					log.Println("sync to dpdk ok")
					return
				}
			}
		}()
	}

	mux := http.NewServeMux()
	mux.HandleFunc("/api/health", h.Health)
	mux.HandleFunc("/api/acl", h.Acl)
	mux.HandleFunc("/api/acl/hits", h.AclHits)
	mux.HandleFunc("/api/acl/", h.AclItem)
	mux.HandleFunc("/api/acl6", h.Acl6)
	mux.HandleFunc("/api/acl6/hits", h.Acl6Hits)
	mux.HandleFunc("/api/acl6/", h.Acl6Item)
	mux.HandleFunc("/api/ifcfg6", h.Ifcfg6)
	mux.HandleFunc("/api/ifcfg6/", h.Ifcfg6Item)
	mux.HandleFunc("/api/ifcfg4", h.Ifcfg4)
	mux.HandleFunc("/api/ifcfg4/", h.Ifcfg4Item)
	mux.HandleFunc("/api/route6", h.Route6)
	mux.HandleFunc("/api/route6/", h.Route6Item)
	mux.HandleFunc("/api/route4", h.Route4)
	mux.HandleFunc("/api/route4/", h.Route4Item)
	mux.HandleFunc("/api/attack", h.Attack)
	mux.HandleFunc("/api/sessions", h.Sessions)
	mux.HandleFunc("/api/sessions6", h.Sessions6)
	mux.HandleFunc("/api/ports", h.PortStats)
	mux.HandleFunc("/api/denies", h.Denies)
	mux.HandleFunc("/api/denies6", h.Denies6)
	mux.HandleFunc("/api/ddos", h.Ddos)

	srv := &http.Server{
		Addr:              addr,
		Handler:           handlers.RequireToken(mux, config.APIToken()),
		ReadHeaderTimeout: 3 * time.Second,
		ReadTimeout:       10 * time.Second,
		WriteTimeout:      15 * time.Second,
		IdleTimeout:       30 * time.Second,
	}

	log.Println("listening", addr)
	log.Fatal(srv.ListenAndServe())
}
