package discovery

import (
	"context"
	"fmt"
	"net"
	"os"
	"strings"

	"github.com/grandcat/zeroconf"
)

const ServiceType = "_claudeusage._tcp"
const Domain = "local."

// Advertise registers the daemon on the LAN. Blocks until ctx is cancelled.
func Advertise(ctx context.Context, instanceName, hostname string, port int, version string, schema int) error {
	host := sanitizeHost(hostname)
	if host == "" {
		raw, _ := os.Hostname()
		host = sanitizeHost(raw)
	}
	ips, err := localIPv4s()
	if err != nil {
		return err
	}

	txt := []string{
		fmt.Sprintf("host=%s.local", host),
		fmt.Sprintf("version=%s", version),
		fmt.Sprintf("schema=%d", schema),
	}
	server, err := zeroconf.RegisterProxy(instanceName, ServiceType, Domain, port, host, ips, txt, nil)
	if err != nil {
		return fmt.Errorf("zeroconf register: %w", err)
	}
	defer server.Shutdown()
	<-ctx.Done()
	return nil
}

// sanitizeHost strips trailing dots and `.local` so zeroconf can append its
// own domain without producing `.local.local`.
func sanitizeHost(h string) string {
	h = strings.TrimSuffix(h, ".")
	h = strings.TrimSuffix(h, ".local")
	return h
}

func localIPv4s() ([]string, error) {
	ifaces, err := net.Interfaces()
	if err != nil {
		return nil, err
	}
	var ips []string
	for _, iface := range ifaces {
		if iface.Flags&net.FlagLoopback != 0 || iface.Flags&net.FlagUp == 0 {
			continue
		}
		addrs, _ := iface.Addrs()
		for _, addr := range addrs {
			var ip net.IP
			switch v := addr.(type) {
			case *net.IPNet:
				ip = v.IP
			case *net.IPAddr:
				ip = v.IP
			}
			if ip == nil || ip.IsLoopback() {
				continue
			}
			if v4 := ip.To4(); v4 != nil {
				ips = append(ips, v4.String())
			}
		}
	}
	if len(ips) == 0 {
		return nil, fmt.Errorf("no non-loopback IPv4 interfaces found")
	}
	return ips, nil
}
