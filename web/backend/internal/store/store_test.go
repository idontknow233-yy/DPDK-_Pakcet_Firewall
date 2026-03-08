package store

import "testing"

func TestAddRejectsInvalidCIDR(t *testing.T) {
	st, err := Open("file::memory:")
	if err != nil {
		t.Fatalf("open store failed: %v", err)
	}
	defer st.Close()

	err = st.Add(Rule{
		Action:     "allow",
		Src:        "10.0.0.1",
		Dst:        "0.0.0.0/0",
		Proto:      6,
		SrcPortMin: 1,
		SrcPortMax: 65535,
		DstPortMin: 80,
		DstPortMax: 80,
	})
	if err == nil {
		t.Fatalf("expected invalid src cidr to fail")
	}

	err = st.Add(Rule{
		Action:     "allow",
		Src:        "10.0.0.0/24",
		Dst:        "invalid",
		Proto:      6,
		SrcPortMin: 1,
		SrcPortMax: 65535,
		DstPortMin: 80,
		DstPortMax: 80,
	})
	if err == nil {
		t.Fatalf("expected invalid dst cidr to fail")
	}
}

func TestAddAcceptsValidCIDR(t *testing.T) {
	st, err := Open("file::memory:")
	if err != nil {
		t.Fatalf("open store failed: %v", err)
	}
	defer st.Close()

	err = st.Add(Rule{
		Action:     "deny",
		Src:        "10.0.0.0/8",
		Dst:        "192.168.1.0/24",
		Proto:      17,
		SrcPortMin: 0,
		SrcPortMax: 65535,
		DstPortMin: 53,
		DstPortMax: 53,
	})
	if err != nil {
		t.Fatalf("expected valid cidr add success, got: %v", err)
	}
}
