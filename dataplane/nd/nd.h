#ifndef DPDK_PF_ND_H
#define DPDK_PF_ND_H

#include <stdint.h>

#include <rte_ether.h>
#include <rte_ip6.h>
#include <rte_mbuf.h>

struct nd_ifcfg {
	struct rte_ipv6_addr ip;
	uint8_t depth;
	uint8_t configured;
	uint16_t reserved;
};

struct nd_table;

struct nd_table *nd_table_create(const char *name, int socket_id, uint32_t capacity, uint32_t timeout_sec, uint32_t req_interval_ms);
void nd_table_free(struct nd_table *t);

int nd_lookup(struct nd_table *t, const struct rte_ipv6_addr *ip, struct rte_ether_addr *mac_out);
void nd_update(struct nd_table *t, const struct rte_ipv6_addr *ip, const struct rte_ether_addr *mac);
int nd_should_request(struct nd_table *t, const struct rte_ipv6_addr *ip);
void nd_mark_requested(struct nd_table *t, const struct rte_ipv6_addr *ip, uint64_t now_tsc);

int nd_process_packet(
	struct nd_table *t,
	struct rte_mbuf *m,
	uint16_t in_port,
	const struct nd_ifcfg *ifcfgs,
	const struct rte_ether_addr *port_macs,
	uint16_t nb_ports,
	uint16_t *tx_port_out
);

struct rte_mbuf *nd_build_ns(
	struct rte_mempool *pool,
	const struct rte_ether_addr *src_mac,
	const struct rte_ipv6_addr *src_ip,
	const struct rte_ipv6_addr *target_ip
);

#endif
