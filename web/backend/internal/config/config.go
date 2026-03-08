package config

import "os"

func CLIAddr() string {
	host := os.Getenv("CLI_HOST")
	if host == "" {
		host = "127.0.0.1"
	}
	port := os.Getenv("CLI_PORT")
	if port == "" {
		port = "8086"
	}
	return host + ":" + port
}

func APIToken() string {
	return os.Getenv("API_TOKEN")
}
