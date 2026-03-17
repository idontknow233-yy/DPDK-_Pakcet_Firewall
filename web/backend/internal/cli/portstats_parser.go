package cli

import (
	"regexp"
	"strconv"
	"strings"
)

type PortStatsRow struct {
	Port    int    `json:"port"`
	Rx      uint64 `json:"rx"`
	Tx      uint64 `json:"tx"`
	Dropped uint64 `json:"dropped"`
	Link    string `json:"link"`
	Speed   uint32 `json:"speed"`
	Duplex  string `json:"duplex"`
	Mac     string `json:"mac"`
}

type PortStatsSnapshot struct {
	Version int64         `json:"version"`
	Mask    uint32        `json:"mask"`
	Ports   []PortStatsRow `json:"ports"`
}

var portStatsHeadRe = regexp.MustCompile(`^PORTSTATS:\s+mask=0x([0-9a-fA-F]+)\s+\(version\s+(\d+)\)$`)
var portStatsItemRe = regexp.MustCompile(`^port=(\d+)\s+rx=(\d+)\s+tx=(\d+)\s+dropped=(\d+)$`)
var portStatsItemExtRe = regexp.MustCompile(`^port=(\d+)\s+rx=(\d+)\s+tx=(\d+)\s+dropped=(\d+)\s+link=(up|down)\s+speed=(\d+)\s+duplex=(full|half)\s+mac=([0-9a-fA-F:]+)$`)

func ParsePortStats(lines []string) (*PortStatsSnapshot, error) {
	var snap PortStatsSnapshot
	for _, l := range lines {
		s := strings.TrimSpace(l)
		if s == "" {
			continue
		}
		if m := portStatsHeadRe.FindStringSubmatch(s); len(m) == 3 {
			mask64, _ := strconv.ParseUint(m[1], 16, 32)
			v, _ := strconv.ParseInt(m[2], 10, 64)
			snap.Mask = uint32(mask64)
			snap.Version = v
			continue
		}
		if m := portStatsItemRe.FindStringSubmatch(s); len(m) == 5 {
			p, _ := strconv.Atoi(m[1])
			rx, _ := strconv.ParseUint(m[2], 10, 64)
			tx, _ := strconv.ParseUint(m[3], 10, 64)
			dr, _ := strconv.ParseUint(m[4], 10, 64)
			snap.Ports = append(snap.Ports, PortStatsRow{
				Port: p, Rx: rx, Tx: tx, Dropped: dr, Link: "unknown", Duplex: "unknown",
			})
			continue
		}
		if m := portStatsItemExtRe.FindStringSubmatch(s); len(m) == 9 {
			p, _ := strconv.Atoi(m[1])
			rx, _ := strconv.ParseUint(m[2], 10, 64)
			tx, _ := strconv.ParseUint(m[3], 10, 64)
			dr, _ := strconv.ParseUint(m[4], 10, 64)
			speed64, _ := strconv.ParseUint(m[6], 10, 32)
			snap.Ports = append(snap.Ports, PortStatsRow{
				Port: p, Rx: rx, Tx: tx, Dropped: dr, Link: m[5], Speed: uint32(speed64), Duplex: m[7], Mac: strings.ToLower(m[8]),
			})
		}
	}
	return &snap, nil
}
