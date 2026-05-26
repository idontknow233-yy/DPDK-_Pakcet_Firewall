package cli

import (
	"regexp"
	"strconv"
	"strings"
)

type Route4Row struct {
	Index int    `json:"index"`
	Dst   string `json:"dst"`
	Nh    string `json:"nh"`
	Port  int    `json:"port"`
}

type Route4Snapshot struct {
	Version int64       `json:"version"`
	Count   int         `json:"count"`
	Routes  []Route4Row `json:"routes"`
}

var route4HeadRe = regexp.MustCompile(`^ROUTE:\s+(\d+)\s+\(version\s+(\d+)\)$`)
var route4ItemRe = regexp.MustCompile(`^(\d+)\s+dst=(\S+)/(\d+)\s+nh=(\S+)\s+port=(\d+)$`)

func ParseRoute4List(lines []string) (*Route4Snapshot, error) {
	var snap Route4Snapshot
	for _, l := range lines {
		s := strings.TrimSpace(l)
		if s == "" {
			continue
		}
		if m := route4HeadRe.FindStringSubmatch(s); len(m) == 3 {
			c, _ := strconv.Atoi(m[1])
			v, _ := strconv.ParseInt(m[2], 10, 64)
			snap.Count = c
			snap.Version = v
			continue
		}
		if m := route4ItemRe.FindStringSubmatch(s); len(m) == 6 {
			idx, _ := strconv.Atoi(m[1])
			dst := m[2] + "/" + m[3]
			nh := m[4]
			port, _ := strconv.Atoi(m[5])
			snap.Routes = append(snap.Routes, Route4Row{Index: idx, Dst: dst, Nh: nh, Port: port})
		}
	}
	return &snap, nil
}
