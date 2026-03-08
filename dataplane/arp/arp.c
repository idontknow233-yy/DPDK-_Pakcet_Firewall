#include "arp.h"

#include <string.h>

#include <rte_arp.h>
#include <rte_byteorder.h>
#include <rte_cycles.h>
#include <rte_hash.h>
#include <rte_jhash.h>
#include <rte_malloc.h>

struct arp_entry {
	struct rte_ether_addr mac;
	uint64_t updated_tsc;
	uint64_t requested_tsc;
	uint8_t has_mac;
};

struct arp_table {
	struct rte_hash *h;
	struct arp_entry *entries;
	uint32_t cap;
	uint64_t timeout_tsc;
	uint64_t req_interval_tsc;
};

static uint64_t sec_to_tsc(uint32_t sec) {
	return (uint64_t)sec * rte_get_timer_hz();
}

static uint64_t ms_to_tsc(uint32_t ms) {
	return (uint64_t)ms * (rte_get_timer_hz() / 1000ULL);
}

struct arp_table *arp_table_create(const char *name, int socket_id, uint32_t capacity, uint32_t timeout_sec, uint32_t req_interval_ms) {
	struct arp_table *t = rte_zmalloc(NULL, sizeof(*t), 0);
	if (!t) {
		return NULL;
	}
	t->cap = capacity ? capacity : 1024;
	t->entries = rte_zmalloc_socket(NULL, sizeof(struct arp_entry) * t->cap, 0, socket_id);
	if (!t->entries) {
		rte_free(t);
		return NULL;
	}

	struct rte_hash_parameters hp;
	memset(&hp, 0, sizeof(hp));
	hp.name = name;
	hp.entries = t->cap;
	hp.key_len = sizeof(uint32_t);
	hp.hash_func = rte_jhash;
	hp.hash_func_init_val = 0;
	hp.socket_id = socket_id;
	t->h = rte_hash_create(&hp);
	if (!t->h) {
		rte_free(t->entries);
		rte_free(t);
		return NULL;
	}

	t->timeout_tsc = sec_to_tsc(timeout_sec ? timeout_sec : 300);
	t->req_interval_tsc = ms_to_tsc(req_interval_ms ? req_interval_ms : 500);
	return t;
}

void arp_table_free(struct arp_table *t) {
	if (!t) {
		return;
	}
	if (t->h) {
		rte_hash_free(t->h);
	}
	if (t->entries) {
		rte_free(t->entries);
	}
	rte_free(t);
}

static struct arp_entry *get_or_create(struct arp_table *t, uint32_t ip) {
	if (!t || !t->h || !t->entries) {
		return NULL;
	}
	int32_t pos = rte_hash_lookup(t->h, &ip);
	if (pos >= 0) {
		return &t->entries[(uint32_t)pos];
	}
	int32_t add_pos = rte_hash_add_key(t->h, &ip);
	if (add_pos < 0) {
		return NULL;
	}
	struct arp_entry *e = &t->entries[(uint32_t)add_pos];
	memset(e, 0, sizeof(*e));
	return e;
}

int arp_lookup(struct arp_table *t, uint32_t ip, struct rte_ether_addr *mac_out) {
	if (!t || !t->h || !t->entries || !mac_out) {
		return -1;
	}
	int32_t pos = rte_hash_lookup(t->h, &ip);
	if (pos < 0) {
		return -1;
	}
	struct arp_entry *e = &t->entries[(uint32_t)pos];
	if (!e->has_mac) {
		return -1;
	}
	uint64_t now = rte_get_timer_cycles();
	if (now - e->updated_tsc > t->timeout_tsc) {
		e->has_mac = 0;
		return -1;
	}
	*mac_out = e->mac;
	return 0;
}

void arp_update(struct arp_table *t, uint32_t ip, const struct rte_ether_addr *mac) {
	if (!t || !mac) {
		return;
	}
	struct arp_entry *e = get_or_create(t, ip);
	if (!e) {
		return;
	}
	e->mac = *mac;
	e->has_mac = 1;
	e->updated_tsc = rte_get_timer_cycles();
}

int arp_should_request(struct arp_table *t, uint32_t ip) {
	if (!t) {
		return 0;
	}
	struct arp_entry *e = get_or_create(t, ip);
	if (!e) {
		return 0;
	}
	uint64_t now = rte_get_timer_cycles();
	if (e->requested_tsc == 0) {
		return 1;
	}
	return (now - e->requested_tsc) > t->req_interval_tsc;
}

void arp_mark_requested(struct arp_table *t, uint32_t ip, uint64_t now_tsc) {
	if (!t) {
		return;
	}
	struct arp_entry *e = get_or_create(t, ip);
	if (!e) {
		return;
	}
	e->requested_tsc = now_tsc;
}

static int is_our_ip(uint32_t ip, uint16_t port, const struct arp_ifcfg *ifcfgs, uint16_t nb_ports) {
	if (!ifcfgs || port >= nb_ports) {
		return 0;
	}
	if (!ifcfgs[port].configured) {
		return 0;
	}
	return ifcfgs[port].ip == ip;
}

int arp_process_packet(
	struct arp_table *t,
	struct rte_mbuf *m,
	uint16_t in_port,
	const struct arp_ifcfg *ifcfgs,
	const struct rte_ether_addr *port_macs,
	uint16_t nb_ports,
	uint16_t *tx_port_out
) {
	if (!m || !port_macs || !tx_port_out) {
		return 0;
	}
	struct rte_ether_hdr *eth = rte_pktmbuf_mtod(m, struct rte_ether_hdr *);
	struct rte_arp_hdr *arp = (struct rte_arp_hdr *)(eth + 1);

	uint16_t op = rte_be_to_cpu_16(arp->arp_opcode);
	uint32_t spa = rte_be_to_cpu_32(arp->arp_data.arp_sip);
	uint32_t tpa = rte_be_to_cpu_32(arp->arp_data.arp_tip);

	arp_update(t, spa, &arp->arp_data.arp_sha);

	if (op == RTE_ARP_OP_REPLY) {
		return 0;
	}

	if (op != RTE_ARP_OP_REQUEST) {
		return 0;
	}

	if (!is_our_ip(tpa, in_port, ifcfgs, nb_ports)) {
		return 0;
	}

	struct rte_ether_addr dst = arp->arp_data.arp_sha;
	struct rte_ether_addr src = port_macs[in_port];

	arp->arp_opcode = rte_cpu_to_be_16(RTE_ARP_OP_REPLY);
	arp->arp_data.arp_tha = arp->arp_data.arp_sha;
	arp->arp_data.arp_tip = arp->arp_data.arp_sip;
	arp->arp_data.arp_sha = src;
	arp->arp_data.arp_sip = rte_cpu_to_be_32(tpa);

	rte_ether_addr_copy(&dst, &eth->dst_addr);
	rte_ether_addr_copy(&src, &eth->src_addr);

	*tx_port_out = in_port;
	return 1;
}

struct rte_mbuf *arp_build_request(
	struct rte_mempool *pool,
	const struct rte_ether_addr *src_mac,
	uint32_t src_ip,
	uint32_t target_ip
) {
	if (!pool || !src_mac) {
		return NULL;
	}
	struct rte_mbuf *m = rte_pktmbuf_alloc(pool);
	if (!m) {
		return NULL;
	}
	const uint16_t pkt_size = sizeof(struct rte_ether_hdr) + sizeof(struct rte_arp_hdr);
	char *data = rte_pktmbuf_append(m, pkt_size);
	if (!data) {
		rte_pktmbuf_free(m);
		return NULL;
	}
	memset(data, 0, pkt_size);

	struct rte_ether_hdr *eth = (struct rte_ether_hdr *)data;
	struct rte_arp_hdr *arp = (struct rte_arp_hdr *)(eth + 1);

	struct rte_ether_addr bcast = { .addr_bytes = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff } };
	rte_ether_addr_copy(&bcast, &eth->dst_addr);
	rte_ether_addr_copy(src_mac, &eth->src_addr);
	eth->ether_type = rte_cpu_to_be_16(RTE_ETHER_TYPE_ARP);

	arp->arp_hardware = rte_cpu_to_be_16(RTE_ARP_HRD_ETHER);
	arp->arp_protocol = rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV4);
	arp->arp_hlen = RTE_ETHER_ADDR_LEN;
	arp->arp_plen = sizeof(uint32_t);
	arp->arp_opcode = rte_cpu_to_be_16(RTE_ARP_OP_REQUEST);

	arp->arp_data.arp_sha = *src_mac;
	arp->arp_data.arp_sip = rte_cpu_to_be_32(src_ip);
	memset(&arp->arp_data.arp_tha, 0, sizeof(arp->arp_data.arp_tha));
	arp->arp_data.arp_tip = rte_cpu_to_be_32(target_ip);

	return m;
}
