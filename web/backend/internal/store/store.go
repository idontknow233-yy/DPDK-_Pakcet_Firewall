package store

import (
	"database/sql"
	"errors"
	"net"
	"strconv"
	"time"

	_ "modernc.org/sqlite"
)

type Rule struct {
	ID         int64
	Action     string
	Src        string
	Dst        string
	Proto      int
	SrcPortMin int
	SrcPortMax int
	DstPortMin int
	DstPortMax int
	CreatedAt  time.Time
}

type Store struct {
	db *sql.DB
}

func Open(path string) (*Store, error) {
	if path == "" {
		path = "./rules.db"
	}
	db, err := sql.Open("sqlite", "file:"+path+"?_pragma=busy_timeout(5000)&_pragma=journal_mode(WAL)")
	if err != nil {
		return nil, err
	}
	s := &Store{db: db}
	if err := s.migrate(); err != nil {
		_ = db.Close()
		return nil, err
	}
	return s, nil
}

func (s *Store) Close() error {
	if s == nil || s.db == nil {
		return nil
	}
	return s.db.Close()
}

func (s *Store) migrate() error {
	_, err := s.db.Exec(`
CREATE TABLE IF NOT EXISTS acl_rules (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  action TEXT NOT NULL,
  src TEXT NOT NULL,
  dst TEXT NOT NULL,
  proto INTEGER NOT NULL,
  src_port_min INTEGER NOT NULL,
  src_port_max INTEGER NOT NULL,
  dst_port_min INTEGER NOT NULL,
  dst_port_max INTEGER NOT NULL,
  created_at TEXT NOT NULL
);`)
	return err
}

func (s *Store) SeedDefaultsIfEmpty() error {
	cnt, err := s.Count()
	if err != nil || cnt != 0 {
		return err
	}
	if err := s.Add(Rule{
		Action:     "deny",
		Src:        "10.0.0.0/8",
		Dst:        "0.0.0.0/0",
		Proto:      0,
		SrcPortMin: 0,
		SrcPortMax: 0,
		DstPortMin: 0,
		DstPortMax: 0,
	}); err != nil {
		return err
	}
	return s.Add(Rule{
		Action:     "allow",
		Src:        "0.0.0.0/0",
		Dst:        "0.0.0.0/0",
		Proto:      0,
		SrcPortMin: 0,
		SrcPortMax: 0,
		DstPortMin: 0,
		DstPortMax: 0,
	})
}

func (s *Store) Count() (int, error) {
	var n int
	err := s.db.QueryRow(`SELECT COUNT(1) FROM acl_rules`).Scan(&n)
	return n, err
}

func (s *Store) List() ([]Rule, error) {
	rows, err := s.db.Query(`SELECT id, action, src, dst, proto, src_port_min, src_port_max, dst_port_min, dst_port_max, created_at FROM acl_rules ORDER BY id ASC`)
	if err != nil {
		return nil, err
	}
	defer rows.Close()
	var out []Rule
	for rows.Next() {
		var r Rule
		var created string
		if err := rows.Scan(&r.ID, &r.Action, &r.Src, &r.Dst, &r.Proto, &r.SrcPortMin, &r.SrcPortMax, &r.DstPortMin, &r.DstPortMax, &created); err != nil {
			return nil, err
		}
		if t, err := time.Parse(time.RFC3339Nano, created); err == nil {
			r.CreatedAt = t
		}
		out = append(out, r)
	}
	return out, rows.Err()
}

func (s *Store) Add(r Rule) error {
	act := r.Action
	if act != "allow" && act != "deny" {
		return errors.New("bad action")
	}
	srcIP, _, err := net.ParseCIDR(r.Src)
	if err != nil {
		return errors.New("bad src cidr")
	}
	dstIP, _, err := net.ParseCIDR(r.Dst)
	if err != nil {
		return errors.New("bad dst cidr")
	}
	if (srcIP.To4() != nil) != (dstIP.To4() != nil) {
		return errors.New("cidr family mismatch")
	}
	if r.Proto < 0 || r.Proto > 255 {
		return errors.New("bad proto")
	}
	if r.SrcPortMin < 0 || r.SrcPortMin > 65535 || r.SrcPortMax < 0 || r.SrcPortMax > 65535 || r.DstPortMin < 0 || r.DstPortMin > 65535 || r.DstPortMax < 0 || r.DstPortMax > 65535 {
		return errors.New("bad port")
	}
	if r.SrcPortMin > r.SrcPortMax || r.DstPortMin > r.DstPortMax {
		return errors.New("bad port range")
	}
	created := time.Now().UTC().Format(time.RFC3339Nano)
	_, err = s.db.Exec(`INSERT INTO acl_rules(action,src,dst,proto,src_port_min,src_port_max,dst_port_min,dst_port_max,created_at) VALUES (?,?,?,?,?,?,?,?,?)`,
		act, r.Src, r.Dst, r.Proto, r.SrcPortMin, r.SrcPortMax, r.DstPortMin, r.DstPortMax, created)
	return err
}

func (s *Store) Clear() error {
	_, err := s.db.Exec(`DELETE FROM acl_rules`)
	return err
}

func (s *Store) ListByFamily(v6 bool) ([]Rule, error) {
	rules, err := s.List()
	if err != nil {
		return nil, err
	}
	out := make([]Rule, 0, len(rules))
	for _, r := range rules {
		ip, _, err := net.ParseCIDR(r.Src)
		if err != nil {
			continue
		}
		is6 := ip.To4() == nil
		if is6 == v6 {
			out = append(out, r)
		}
	}
	return out, nil
}

func (s *Store) ClearByFamily(v6 bool) error {
	rules, err := s.ListByFamily(v6)
	if err != nil {
		return err
	}
	for _, r := range rules {
		if _, err := s.db.Exec(`DELETE FROM acl_rules WHERE id = ?`, r.ID); err != nil {
			return err
		}
	}
	return nil
}

func (s *Store) DeleteByIndex(index int) error {
	rules, err := s.List()
	if err != nil {
		return err
	}
	if index < 0 || index >= len(rules) {
		return errors.New("index out of range")
	}
	_, err = s.db.Exec(`DELETE FROM acl_rules WHERE id = ?`, rules[index].ID)
	return err
}

func (s *Store) DeleteByIndexByFamily(index int, v6 bool) error {
	rules, err := s.ListByFamily(v6)
	if err != nil {
		return err
	}
	if index < 0 || index >= len(rules) {
		return errors.New("index out of range")
	}
	_, err = s.db.Exec(`DELETE FROM acl_rules WHERE id = ?`, rules[index].ID)
	return err
}

func EnvInt(v string, def int) int {
	n, err := strconv.Atoi(v)
	if err != nil {
		return def
	}
	return n
}
