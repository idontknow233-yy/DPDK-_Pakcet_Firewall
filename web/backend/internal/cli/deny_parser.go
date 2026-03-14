package cli

import (
	"regexp"
	"strconv"
	"strings"
)

type DenyRow struct {
	Index   int    `json:"index"`
	AgeMs   uint64 `json:"age_ms"`
	InPort  int    `json:"in_port"`
	Proto   int    `json:"proto"`
	Src     string `json:"src"`
	Dst     string `json:"dst"`
	Rule    uint32 `json:"rule"`
}

type DenySnapshot struct {
	Version int64     `json:"version"`
	Count   int       `json:"count"`
	Denies  []DenyRow `json:"denies"`
}

var denyHeadRe = regexp.MustCompile(`^DENIES:\s+(\d+)\s+\(version\s+(\d+)\)$`)
var denyItemRe = regexp.MustCompile(`^(\d+)\s+age_ms=(\d+)\s+in_port=(\d+)\s+proto=(\d+)\s+src=([0-9.]+:\d+)\s+dst=([0-9.]+:\d+)\s+rule=(\d+)$`)

func ParseDenyList(lines []string) (*DenySnapshot, error) {
	var snap DenySnapshot
	for _, l := range lines {
		s := strings.TrimSpace(l)
		if s == "" {
			continue
		}
		if m := denyHeadRe.FindStringSubmatch(s); len(m) == 3 {
			c, _ := strconv.Atoi(m[1])
			v, _ := strconv.ParseInt(m[2], 10, 64)
			snap.Count = c
			snap.Version = v
			continue
		}
		if m := denyItemRe.FindStringSubmatch(s); len(m) == 8 {
			idx, _ := strconv.Atoi(m[1])
			age, _ := strconv.ParseUint(m[2], 10, 64)
			inp, _ := strconv.Atoi(m[3])
			proto, _ := strconv.Atoi(m[4])
			rule64, _ := strconv.ParseUint(m[7], 10, 32)
			snap.Denies = append(snap.Denies, DenyRow{
				Index:  idx,
				AgeMs:  age,
				InPort: inp,
				Proto:  proto,
				Src:    m[5],
				Dst:    m[6],
				Rule:   uint32(rule64),
			})
		}
	}
	return &snap, nil
}

