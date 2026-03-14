package cli

import (
	"regexp"
	"strconv"
	"strings"
)

type Ifcfg6Row struct {
	Port int    `json:"port"`
	CIDR string `json:"cidr"`
}

type Ifcfg6Snapshot struct {
	Version int64      `json:"version"`
	Count   int        `json:"count"`
	Ifaces  []Ifcfg6Row `json:"ifaces"`
}

type Route6Row struct {
	Index int    `json:"index"`
	Dst   string `json:"dst"`
	Nh    string `json:"nh"`
	Port  int    `json:"port"`
}

type Route6Snapshot struct {
	Version int64       `json:"version"`
	Count   int         `json:"count"`
	Routes  []Route6Row `json:"routes"`
}

var ifcfg6HeadRe = regexp.MustCompile(`^IFCFG6:\s+(\d+)\s+\(version\s+(\d+)\)$`)
var ifcfg6ItemRe = regexp.MustCompile(`^port=(\d+)\s+ip=([0-9a-fA-F:]+)/(\d+)$`)

func ParseIfcfg6Show(lines []string) (*Ifcfg6Snapshot, error) {
	var snap Ifcfg6Snapshot
	for _, l := range lines {
		s := strings.TrimSpace(l)
		if s == "" {
			continue
		}
		if m := ifcfg6HeadRe.FindStringSubmatch(s); len(m) == 3 {
			c, _ := strconv.Atoi(m[1])
			v, _ := strconv.ParseInt(m[2], 10, 64)
			snap.Count = c
			snap.Version = v
			continue
		}
		if m := ifcfg6ItemRe.FindStringSubmatch(s); len(m) == 4 {
			p, _ := strconv.Atoi(m[1])
			cidr := m[2] + "/" + m[3]
			snap.Ifaces = append(snap.Ifaces, Ifcfg6Row{Port: p, CIDR: cidr})
		}
	}
	return &snap, nil
}

var route6HeadRe = regexp.MustCompile(`^ROUTE6:\s+(\d+)\s+\(version\s+(\d+)\)$`)
var route6ItemRe = regexp.MustCompile(`^(\d+)\s+dst=([0-9a-fA-F:]+)/(\d+)\s+nh=([0-9a-fA-F:]+)\s+port=(\d+)$`)

func ParseRoute6List(lines []string) (*Route6Snapshot, error) {
	var snap Route6Snapshot
	for _, l := range lines {
		s := strings.TrimSpace(l)
		if s == "" {
			continue
		}
		if m := route6HeadRe.FindStringSubmatch(s); len(m) == 3 {
			c, _ := strconv.Atoi(m[1])
			v, _ := strconv.ParseInt(m[2], 10, 64)
			snap.Count = c
			snap.Version = v
			continue
		}
		if m := route6ItemRe.FindStringSubmatch(s); len(m) == 6 {
			idx, _ := strconv.Atoi(m[1])
			dst := m[2] + "/" + m[3]
			nh := m[4]
			port, _ := strconv.Atoi(m[5])
			snap.Routes = append(snap.Routes, Route6Row{Index: idx, Dst: dst, Nh: nh, Port: port})
		}
	}
	return &snap, nil
}

