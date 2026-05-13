package discovery

import (
	"context"
	"fmt"

	"github.com/grandcat/zeroconf"
)

const ServiceType = "_claudeusage._tcp"
const Domain = "local."

// Advertise registers the daemon on the LAN. Blocks until ctx is cancelled.
func Advertise(ctx context.Context, instanceName, hostname string, port int, version string, schema int) error {
	txt := []string{
		fmt.Sprintf("host=%s", hostname),
		fmt.Sprintf("version=%s", version),
		fmt.Sprintf("schema=%d", schema),
	}
	server, err := zeroconf.Register(instanceName, ServiceType, Domain, port, txt, nil)
	if err != nil {
		return fmt.Errorf("zeroconf register: %w", err)
	}
	defer server.Shutdown()
	<-ctx.Done()
	return nil
}
