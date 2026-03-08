package cli

import (
	"bytes"
	"net"
	"strings"
	"time"
)

type Client struct {
	Addr string
}

func (c *Client) Run(cmd string) ([]string, error) {
	conn, err := net.DialTimeout("tcp", c.Addr, 2*time.Second)
	if err != nil {
		return nil, err
	}
	defer conn.Close()
	_ = conn.SetDeadline(time.Now().Add(3 * time.Second))
	if _, err := readUntilPrompt(conn); err != nil {
		return nil, err
	}
	if _, err := conn.Write([]byte(cmd + "\n")); err != nil {
		return nil, err
	}
	lines, err := readUntilPrompt(conn)
	if err != nil {
		return nil, err
	}
	var out []string
	for _, l := range lines {
		if strings.TrimSpace(l) == "pipeline>" {
			continue
		}
		if strings.TrimSpace(l) == cmd {
			continue
		}
		out = append(out, l)
	}
	return out, nil
}

func readUntilPrompt(conn net.Conn) ([]string, error) {
	var buf bytes.Buffer
	tmp := make([]byte, 4096)
	for {
		n, err := conn.Read(tmp)
		if n > 0 {
			buf.Write(tmp[:n])
			if bytes.Contains(buf.Bytes(), []byte("pipeline>")) {
				break
			}
		}
		if err != nil {
			return splitLines(buf.Bytes()), err
		}
	}
	return splitLines(buf.Bytes()), nil
}

func splitLines(b []byte) []string {
	b = bytes.ReplaceAll(b, []byte("\r\n"), []byte("\n"))
	parts := bytes.Split(b, []byte("\n"))
	out := make([]string, 0, len(parts))
	for _, p := range parts {
		s := string(p)
		if s == "" {
			continue
		}
		out = append(out, s)
	}
	return out
}
