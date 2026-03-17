package cli

import (
	"regexp"
	"strconv"
	"strings"
)

type AclHitsSnapshot struct {
	Version     int64    `json:"version"`
	RuleVersion int64    `json:"rule_version"`
	Count       int      `json:"count"`
	Pkts        []uint64 `json:"pkts"`
	Bytes       []uint64 `json:"bytes"`
}

var aclHitsHeadRe = regexp.MustCompile(`^ACLHITS:\s+count=(\d+)\s+rule_version=(\d+)\s+\(version\s+(\d+)\)$`)
var aclHitsItemRe = regexp.MustCompile(`^(\d+)\s+pkts=(\d+)\s+bytes=(\d+)$`)

func ParseAclHits(lines []string) (*AclHitsSnapshot, error) {
	var snap AclHitsSnapshot
	for _, l := range lines {
		s := strings.TrimSpace(l)
		if s == "" {
			continue
		}
		if m := aclHitsHeadRe.FindStringSubmatch(s); len(m) == 4 {
			c, _ := strconv.Atoi(m[1])
			rv, _ := strconv.ParseInt(m[2], 10, 64)
			v, _ := strconv.ParseInt(m[3], 10, 64)
			snap.Count = c
			snap.RuleVersion = rv
			snap.Version = v
			if c > 0 {
				snap.Pkts = make([]uint64, c)
				snap.Bytes = make([]uint64, c)
			}
			continue
		}
		if m := aclHitsItemRe.FindStringSubmatch(s); len(m) == 4 {
			idx, _ := strconv.Atoi(m[1])
			pk, _ := strconv.ParseUint(m[2], 10, 64)
			by, _ := strconv.ParseUint(m[3], 10, 64)
			if idx >= 0 && idx < len(snap.Pkts) {
				snap.Pkts[idx] = pk
				snap.Bytes[idx] = by
			}
		}
	}
	return &snap, nil
}

var acl6HitsHeadRe = regexp.MustCompile(`^ACL6HITS:\s+count=(\d+)\s+rule_version=(\d+)\s+\(version\s+(\d+)\)$`)
var acl6HitsItemRe = regexp.MustCompile(`^(\d+)\s+pkts=(\d+)\s+bytes=(\d+)$`)

func ParseAcl6Hits(lines []string) (*AclHitsSnapshot, error) {
	var snap AclHitsSnapshot
	for _, l := range lines {
		s := strings.TrimSpace(l)
		if s == "" {
			continue
		}
		if m := acl6HitsHeadRe.FindStringSubmatch(s); len(m) == 4 {
			c, _ := strconv.Atoi(m[1])
			rv, _ := strconv.ParseInt(m[2], 10, 64)
			v, _ := strconv.ParseInt(m[3], 10, 64)
			snap.Count = c
			snap.RuleVersion = rv
			snap.Version = v
			if c > 0 {
				snap.Pkts = make([]uint64, c)
				snap.Bytes = make([]uint64, c)
			}
			continue
		}
		if m := acl6HitsItemRe.FindStringSubmatch(s); len(m) == 4 {
			idx, _ := strconv.Atoi(m[1])
			pk, _ := strconv.ParseUint(m[2], 10, 64)
			by, _ := strconv.ParseUint(m[3], 10, 64)
			if idx >= 0 && idx < len(snap.Pkts) {
				snap.Pkts[idx] = pk
				snap.Bytes[idx] = by
			}
		}
	}
	return &snap, nil
}

