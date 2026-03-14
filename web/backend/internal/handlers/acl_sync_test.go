package handlers

import (
	"strconv"
	"strings"
	"testing"

	"dpdk-packet-firewall-web-backend/internal/store"
)

type fakeCLIRunner struct {
	rules []store.Rule
	cmds  []string
}

func (f *fakeCLIRunner) Run(cmd string) ([]string, error) {
	f.cmds = append(f.cmds, cmd)
	if cmd == "acl list" {
		return f.renderList(), nil
	}
	if cmd == "acl6 list" {
		return []string{"ACL6 rules: 0 (version 1)"}, nil
	}
	if cmd == "acl clear" {
		f.rules = nil
		return []string{"ACL cleared"}, nil
	}
	if cmd == "acl6 clear" {
		return []string{"ACL6 cleared"}, nil
	}
	if strings.HasPrefix(cmd, "acl del ") {
		idx, err := strconv.Atoi(strings.TrimSpace(strings.TrimPrefix(cmd, "acl del ")))
		if err != nil || idx < 0 || idx >= len(f.rules) {
			return []string{"delete failed"}, nil
		}
		f.rules = append(f.rules[:idx], f.rules[idx+1:]...)
		return []string{"ACL rule deleted"}, nil
	}
	if strings.HasPrefix(cmd, "acl6 del ") {
		return []string{"ACL6 rule deleted"}, nil
	}
	if strings.HasPrefix(cmd, "acl add ") {
		r, err := parseAddCommand(cmd)
		if err != nil {
			return []string{"add failed"}, nil
		}
		f.rules = append(f.rules, r)
		return []string{"ACL rule added"}, nil
	}
	if strings.HasPrefix(cmd, "acl6 add ") {
		return []string{"ACL6 rule added"}, nil
	}
	return []string{"unknown"}, nil
}

func (f *fakeCLIRunner) renderList() []string {
	lines := []string{"ACL rules: " + strconv.Itoa(len(f.rules)) + " (version 1)"}
	for i, r := range f.rules {
		if r.Proto == 0 && r.SrcPortMin == 0 && r.SrcPortMax == 0 && r.DstPortMin == 0 && r.DstPortMax == 0 {
			lines = append(lines, strconv.Itoa(i)+" "+r.Action+" src="+r.Src+" dst="+r.Dst+" proto=any ports=any")
			continue
		}
		lines = append(lines,
			strconv.Itoa(i)+" "+r.Action+" src="+r.Src+" dst="+r.Dst+" proto="+strconv.Itoa(r.Proto)+
				" sport="+strconv.Itoa(r.SrcPortMin)+"-"+strconv.Itoa(r.SrcPortMax)+
				" dport="+strconv.Itoa(r.DstPortMin)+"-"+strconv.Itoa(r.DstPortMax),
		)
	}
	return lines
}

func parseAddCommand(cmd string) (store.Rule, error) {
	parts := strings.Fields(cmd)
	if len(parts) != 10 || parts[0] != "acl" || parts[1] != "add" {
		return store.Rule{}, strconv.ErrSyntax
	}
	proto, err := strconv.Atoi(parts[5])
	if err != nil {
		return store.Rule{}, err
	}
	spMin, err := strconv.Atoi(parts[6])
	if err != nil {
		return store.Rule{}, err
	}
	spMax, err := strconv.Atoi(parts[7])
	if err != nil {
		return store.Rule{}, err
	}
	dpMin, err := strconv.Atoi(parts[8])
	if err != nil {
		return store.Rule{}, err
	}
	dpMax, err := strconv.Atoi(parts[9])
	if err != nil {
		return store.Rule{}, err
	}
	return store.Rule{
		Action:     parts[2],
		Src:        parts[3],
		Dst:        parts[4],
		Proto:      proto,
		SrcPortMin: spMin,
		SrcPortMax: spMax,
		DstPortMin: dpMin,
		DstPortMax: dpMax,
	}, nil
}

func TestSyncToDpdkUsesIncrementalWhenPrefixMatches(t *testing.T) {
	st, err := store.Open("file::memory:")
	if err != nil {
		t.Fatalf("open store failed: %v", err)
	}
	defer st.Close()

	r1 := store.Rule{Action: "allow", Src: "10.0.0.0/8", Dst: "192.168.1.0/24", Proto: 6, SrcPortMin: 1000, SrcPortMax: 1000, DstPortMin: 80, DstPortMax: 80}
	r2 := store.Rule{Action: "deny", Src: "0.0.0.0/0", Dst: "10.10.0.0/16", Proto: 17, SrcPortMin: 0, SrcPortMax: 65535, DstPortMin: 53, DstPortMax: 53}
	if err := st.Add(r1); err != nil {
		t.Fatalf("add rule1 failed: %v", err)
	}
	if err := st.Add(r2); err != nil {
		t.Fatalf("add rule2 failed: %v", err)
	}

	oldTail := store.Rule{Action: "allow", Src: "172.16.0.0/12", Dst: "0.0.0.0/0", Proto: 0, SrcPortMin: 0, SrcPortMax: 0, DstPortMin: 0, DstPortMax: 0}
	fake := &fakeCLIRunner{rules: []store.Rule{r1, oldTail}}

	h := &Handler{cli: fake, store: st}
	if err := h.SyncToDpdk(); err != nil {
		t.Fatalf("sync failed: %v", err)
	}

	if len(fake.rules) != 2 || !equalRule(fake.rules[0], r1) || !equalRule(fake.rules[1], r2) {
		t.Fatalf("unexpected synced rules: %+v", fake.rules)
	}
	cmdLog := strings.Join(fake.cmds, "\n")
	if strings.Contains(cmdLog, "acl clear") {
		t.Fatalf("unexpected full replay, cmds=%s", cmdLog)
	}
	if !strings.Contains(cmdLog, "acl del 1") {
		t.Fatalf("expected delete tail rule, cmds=%s", cmdLog)
	}
	if !strings.Contains(cmdLog, "acl add deny 0.0.0.0/0 10.10.0.0/16 17 0 65535 53 53") {
		t.Fatalf("expected append desired tail, cmds=%s", cmdLog)
	}
}
