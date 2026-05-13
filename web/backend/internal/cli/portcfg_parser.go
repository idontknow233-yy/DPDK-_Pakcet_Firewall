package cli

import (
	"regexp"
	"strconv"
	"strings"
)

type Ifcfg4Row struct {
	Port int    `json:"port"`
	CIDR string `json:"cidr"`
}

type Ifcfg4Snapshot struct {
	Version int64       `json:"version"`
	Count  int         `json:"count"`
	Ifaces []Ifcfg4Row `json:"ifaces"`
}

var ifcfg4HeadRe = regexp.MustCompile(`^IFCFG4:\s+(\d+)\s+\(version\s+(\d+)\)$`)
var ifcfg4ItemRe = regexp.MustCompile(`^port=(\d+)\s+ip=([0-9.]+)/(\d+)$`)

func ParseIfcfg4Show(lines []string) (*Ifcfg4Snapshot, error) {
	var snap Ifcfg4Snapshot
	for _, l := range lines {
		s := strings.TrimSpace(l)
		if s == "" {
			continue
		}
		if m := ifcfg4HeadRe.FindStringSubmatch(s); len(m) == 3 {
			c, _ := strconv.Atoi(m[1])
			v, _ := strconv.ParseInt(m[2], 10, 64)
			snap.Count = c
			snap.Version = v
			continue
		}
		if m := ifcfg4ItemRe.FindStringSubmatch(s); len(m) == 4 {
			p, _ := strconv.Atoi(m[1])
			cidr := m[2] + "/" + m[3]
			snap.Ifaces = append(snap.Ifaces, Ifcfg4Row{Port: p, CIDR: cidr})
		}
	}
	return &snap, nil
}
