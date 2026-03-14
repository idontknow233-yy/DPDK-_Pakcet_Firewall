package cli

import (
	"regexp"
	"strconv"
	"strings"
)

var sess6HeadRe = regexp.MustCompile(`^SESSIONS6:\s+(\d+)\s+\(version\s+(\d+)\)$`)
var sess6ItemRe = regexp.MustCompile(`^(\d+)\s+proto=(\d+)\s+src=\[([0-9a-fA-F:]+)\]:(\d+)\s+dst=\[([0-9a-fA-F:]+)\]:(\d+)\s+packets=(\d+)\s+bytes=(\d+)\s+last_seen_ms=(\d+)$`)

func ParseSession6List(lines []string) (*SessionSnapshot, error) {
	var snap SessionSnapshot
	for _, l := range lines {
		s := strings.TrimSpace(l)
		if s == "" {
			continue
		}
		if m := sess6HeadRe.FindStringSubmatch(s); len(m) == 3 {
			c, _ := strconv.Atoi(m[1])
			v, _ := strconv.ParseInt(m[2], 10, 64)
			snap.Count = c
			snap.Version = v
			continue
		}
		if m := sess6ItemRe.FindStringSubmatch(s); len(m) == 11 {
			idx, _ := strconv.Atoi(m[1])
			proto, _ := strconv.Atoi(m[2])
			pk, _ := strconv.ParseUint(m[7], 10, 64)
			by, _ := strconv.ParseUint(m[8], 10, 64)
			ms, _ := strconv.ParseUint(m[9], 10, 64)
			snap.Sessions = append(snap.Sessions, SessionRow{
				Index:      idx,
				Proto:      proto,
				Src:        "[" + m[3] + "]:" + m[4],
				Dst:        "[" + m[5] + "]:" + m[6],
				Packets:    pk,
				Bytes:      by,
				LastSeenMs: ms,
			})
		}
	}
	return &snap, nil
}

