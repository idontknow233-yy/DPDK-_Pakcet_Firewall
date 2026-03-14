package cli

import (
	"regexp"
	"strconv"
	"strings"
)

type DdosConfig struct {
	Version   int64  `json:"version"`
	SynPps    uint32 `json:"syn_pps"`
	SynBurst  uint32 `json:"syn_burst"`
	UdpPps    uint32 `json:"udp_pps"`
	UdpBurst  uint32 `json:"udp_burst"`
}

var ddosRe = regexp.MustCompile(`^DDOS:\s+syn_pps=(\d+)\s+syn_burst=(\d+)\s+udp_pps=(\d+)\s+udp_burst=(\d+)\s+\(version\s+(\d+)\)$`)

func ParseDdosShow(lines []string) (*DdosConfig, error) {
	for _, l := range lines {
		s := strings.TrimSpace(l)
		if s == "" {
			continue
		}
		if m := ddosRe.FindStringSubmatch(s); len(m) == 6 {
			synPps, _ := strconv.ParseUint(m[1], 10, 32)
			synBurst, _ := strconv.ParseUint(m[2], 10, 32)
			udpPps, _ := strconv.ParseUint(m[3], 10, 32)
			udpBurst, _ := strconv.ParseUint(m[4], 10, 32)
			v, _ := strconv.ParseInt(m[5], 10, 64)
			return &DdosConfig{
				Version:  v,
				SynPps:   uint32(synPps),
				SynBurst: uint32(synBurst),
				UdpPps:   uint32(udpPps),
				UdpBurst: uint32(udpBurst),
			}, nil
		}
	}
	return &DdosConfig{}, nil
}

