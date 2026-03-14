package cli

import (
	"regexp"
	"strconv"
	"strings"
)

var deny6HeadRe = regexp.MustCompile(`^DENIES6:\s+(\d+)\s+\(version\s+(\d+)\)$`)
var deny6ItemRe = regexp.MustCompile(`^(\d+)\s+age_ms=(\d+)\s+in_port=(\d+)\s+proto=(\d+)\s+src=\[([0-9a-fA-F:]+)\]:(\d+)\s+dst=\[([0-9a-fA-F:]+)\]:(\d+)\s+rule=(\d+)$`)

func ParseDeny6List(lines []string) (*DenySnapshot, error) {
	var snap DenySnapshot
	for _, l := range lines {
		s := strings.TrimSpace(l)
		if s == "" {
			continue
		}
		if m := deny6HeadRe.FindStringSubmatch(s); len(m) == 3 {
			c, _ := strconv.Atoi(m[1])
			v, _ := strconv.ParseInt(m[2], 10, 64)
			snap.Count = c
			snap.Version = v
			continue
		}
		if m := deny6ItemRe.FindStringSubmatch(s); len(m) == 10 {
			idx, _ := strconv.Atoi(m[1])
			age, _ := strconv.ParseUint(m[2], 10, 64)
			inp, _ := strconv.Atoi(m[3])
			proto, _ := strconv.Atoi(m[4])
			rule64, _ := strconv.ParseUint(m[9], 10, 32)
			snap.Denies = append(snap.Denies, DenyRow{
				Index:  idx,
				AgeMs:  age,
				InPort: inp,
				Proto:  proto,
				Src:    "[" + m[5] + "]:" + m[6],
				Dst:    "[" + m[7] + "]:" + m[8],
				Rule:   uint32(rule64),
			})
		}
	}
	return &snap, nil
}

