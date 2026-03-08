package cli

import (
	"regexp"
	"strconv"
	"strings"
)

type SessionRow struct {
	Index       int    `json:"index"`
	Proto       int    `json:"proto"`
	Src         string `json:"src"`
	Dst         string `json:"dst"`
	Packets     uint64 `json:"packets"`
	Bytes       uint64 `json:"bytes"`
	LastSeenMs  uint64 `json:"last_seen_ms"`
}

type SessionSnapshot struct {
	Version int64        `json:"version"`
	Count   int          `json:"count"`
	Sessions []SessionRow `json:"sessions"`
}

var sessHeadRe = regexp.MustCompile(`^SESSIONS:\s+(\d+)\s+\(version\s+(\d+)\)$`)
var sessItemRe = regexp.MustCompile(`^(\d+)\s+proto=(\d+)\s+src=([0-9.]+:\d+)\s+dst=([0-9.]+:\d+)\s+packets=(\d+)\s+bytes=(\d+)\s+last_seen_ms=(\d+)$`)

func ParseSessionList(lines []string) (*SessionSnapshot, error) {
	var snap SessionSnapshot
	for _, l := range lines {
		s := strings.TrimSpace(l)
		if s == "" {
			continue
		}
		if m := sessHeadRe.FindStringSubmatch(s); len(m) == 3 {
			c, _ := strconv.Atoi(m[1])
			v, _ := strconv.ParseInt(m[2], 10, 64)
			snap.Count = c
			snap.Version = v
			continue
		}
		if m := sessItemRe.FindStringSubmatch(s); len(m) == 8 {
			idx, _ := strconv.Atoi(m[1])
			proto, _ := strconv.Atoi(m[2])
			pk, _ := strconv.ParseUint(m[5], 10, 64)
			by, _ := strconv.ParseUint(m[6], 10, 64)
			ms, _ := strconv.ParseUint(m[7], 10, 64)
			snap.Sessions = append(snap.Sessions, SessionRow{
				Index: idx, Proto: proto, Src: m[3], Dst: m[4],
				Packets: pk, Bytes: by, LastSeenMs: ms,
			})
		}
	}
	return &snap, nil
}

