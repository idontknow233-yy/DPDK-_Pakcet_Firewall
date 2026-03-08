package cli

import (
	"regexp"
	"strconv"
	"strings"
)

type Rule struct {
	Index int
	Action string
	Src string
	Dst string
	Proto string
	Sport string
	Dport string
}

type Snapshot struct {
	Version int64
	Count int
	Rules []Rule
}

var headRe = regexp.MustCompile(`ACL rules: (\d+) \(version (\d+)\)`)
var itemPortsRe = regexp.MustCompile(`^(\d+)\s+(allow|deny)\s+src=(.+)\s+dst=(.+)\s+proto=(\d+)\s+sport=(\d+)-(\d+)\s+dport=(\d+)-(\d+)$`)
var itemAnyRe = regexp.MustCompile(`^(\d+)\s+(allow|deny)\s+src=(.+)\s+dst=(.+)\s+proto=any\s+ports=any$`)

func ParseAclList(lines []string) (*Snapshot, error) {
	var snap Snapshot
	for _, l := range lines {
		s := strings.TrimSpace(l)
		m := headRe.FindStringSubmatch(s)
		if len(m) == 3 {
			c, _ := strconv.Atoi(m[1])
			v, _ := strconv.ParseInt(m[2], 10, 64)
			snap.Count = c
			snap.Version = v
			continue
		}
		if m := itemPortsRe.FindStringSubmatch(s); len(m) == 10 {
			idx, _ := strconv.Atoi(m[1])
			snap.Rules = append(snap.Rules, Rule{
				Index: idx, Action: m[2], Src: m[3], Dst: m[4],
				Proto: m[5], Sport: m[6] + "-" + m[7], Dport: m[8] + "-" + m[9],
			})
			continue
		}
		if m := itemAnyRe.FindStringSubmatch(s); len(m) == 5 {
			idx, _ := strconv.Atoi(m[1])
			snap.Rules = append(snap.Rules, Rule{
				Index: idx, Action: m[2], Src: m[3], Dst: m[4],
				Proto: "any", Sport: "any", Dport: "any",
			})
			continue
		}
	}
	return &snap, nil
}
