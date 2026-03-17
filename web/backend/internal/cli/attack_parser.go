package cli

import (
	"regexp"
	"strconv"
	"strings"
)

type AttackSnapshot struct {
	Version       int64  `json:"version"`
	Mitigation    int    `json:"mitigation"`
	ScanPortsSec  int    `json:"scan_ports_sec"`
	BanSec        int    `json:"ban_sec"`
	SynPps        int    `json:"syn_pps"`
	UdpPps        int    `json:"udp_pps"`
	ScanEvents    int    `json:"scan_events"`
	ScanBanned    int    `json:"scan_banned"`
	Top4          string `json:"top4"`
	Top4Ports     int    `json:"top4_ports"`
	Top6          string `json:"top6"`
	Top6Ports     int    `json:"top6_ports"`
}

var attackHeadRe = regexp.MustCompile(`^ATTACK:\s+mitigation=(\d+)\s+scan_ports_sec=(\d+)\s+ban_sec=(\d+)\s+syn_pps=(\d+)\s+udp_pps=(\d+)\s+scan_events=(\d+)\s+scan_banned=(\d+)\s+top4=([^/]+)/(\d+)\s+top6=([^/]+)/(\d+)\s+\(version\s+(\d+)\)$`)

func ParseAttackShow(lines []string) (*AttackSnapshot, error) {
	for _, l := range lines {
		s := strings.TrimSpace(l)
		if s == "" {
			continue
		}
		if m := attackHeadRe.FindStringSubmatch(s); len(m) == 13 {
			mit, _ := strconv.Atoi(m[1])
			scan, _ := strconv.Atoi(m[2])
			ban, _ := strconv.Atoi(m[3])
			syn, _ := strconv.Atoi(m[4])
			udp, _ := strconv.Atoi(m[5])
			ev, _ := strconv.Atoi(m[6])
			bd, _ := strconv.Atoi(m[7])
			top4Ports, _ := strconv.Atoi(m[9])
			top6Ports, _ := strconv.Atoi(m[11])
			ver, _ := strconv.ParseInt(m[12], 10, 64)
			return &AttackSnapshot{
				Version: ver, Mitigation: mit, ScanPortsSec: scan, BanSec: ban,
				SynPps: syn, UdpPps: udp, ScanEvents: ev, ScanBanned: bd,
				Top4: m[8], Top4Ports: top4Ports, Top6: m[10], Top6Ports: top6Ports,
			}, nil
		}
	}
	return &AttackSnapshot{}, nil
}

