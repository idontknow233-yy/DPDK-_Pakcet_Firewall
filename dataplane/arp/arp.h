#ifndef DPDK_PF_ARP_H
#define DPDK_PF_ARP_H

#include <stdint.h>

#include <rte_ether.h>
#include <rte_mbuf.h>

struct arp_ifcfg {
	uint32_t ip;
	uint32_t mask;
	uint8_t configured;
};

struct arp_table;

struct arp_table *arp_table_create(const char *name, int socket_id, uint32_t capacity, uint32_t timeout_sec, uint32_t req_interval_ms);
void arp_table_free(struct arp_table *t);

int arp_lookup(struct arp_table *t, uint32_t ip, struct rte_ether_addr *mac_out);
void arp_update(struct arp_table *t, uint32_t ip, const struct rte_ether_addr *mac);

int arp_should_request(struct arp_table *t, uint32_t ip);
void arp_mark_requested(struct arp_table *t, uint32_t ip, uint64_t now_tsc);

int arp_process_packet(
	struct arp_table *t,
	struct rte_mbuf *m,
	uint16_t in_port,
	const struct arp_ifcfg *ifcfgs,
	const struct rte_ether_addr *port_macs,
	uint16_t nb_ports,
	uint16_t *tx_port_out
);

struct rte_mbuf *arp_build_request(
	struct rte_mempool *pool,
	const struct rte_ether_addr *src_mac,
	uint32_t src_ip,
	uint32_t target_ip
);

#endif
