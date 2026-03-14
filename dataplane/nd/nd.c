#include "nd.h"

#include <string.h>

#include <netinet/in.h>

#include <rte_byteorder.h>
#include <rte_cksum.h>
#include <rte_cycles.h>
#include <rte_hash.h>
#include <rte_icmp.h>
#include <rte_jhash.h>
#include <rte_malloc.h>

#define ICMPV6_TYPE_NS 135
#define ICMPV6_TYPE_NA 136

#define ND_OPT_SLLA 1
#define ND_OPT_TLLA 2

struct nd_entry {
	struct rte_ether_addr mac;
	uint64_t updated_tsc;
	uint64_t requested_tsc;
	uint8_t has_mac;
};

struct nd_table {
	struct rte_hash *h;
	struct nd_entry *entries;
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

static uint16_t cksum_reduce(uint32_t sum) {
	sum = ((sum & 0xffff0000u) >> 16) + (sum & 0xffffu);
	sum = ((sum & 0xffff0000u) >> 16) + (sum & 0xffffu);
	return (uint16_t)sum;
}

static uint16_t icmpv6_cksum(const struct rte_ipv6_hdr *ip6, const void *l4, uint16_t l4_len) {
	uint32_t sum = 0;
	sum = __rte_raw_cksum(&ip6->src_addr, sizeof(ip6->src_addr), sum);
	sum = __rte_raw_cksum(&ip6->dst_addr, sizeof(ip6->dst_addr), sum);
	uint32_t len32 = rte_cpu_to_be_32(l4_len);
	sum = __rte_raw_cksum(&len32, sizeof(len32), sum);
	uint32_t nh = rte_cpu_to_be_32(IPPROTO_ICMPV6);
	sum = __rte_raw_cksum(&nh, sizeof(nh), sum);
	sum = __rte_raw_cksum(l4, l4_len, sum);
	uint16_t r = cksum_reduce(sum);
	return (uint16_t)~r;
}

struct nd_table *nd_table_create(const char *name, int socket_id, uint32_t capacity, uint32_t timeout_sec, uint32_t req_interval_ms) {
	struct nd_table *t = rte_zmalloc(NULL, sizeof(*t), 0);
	if (!t) {
		return NULL;
	}
	t->cap = capacity ? capacity : 1024;
	t->entries = rte_zmalloc_socket(NULL, sizeof(struct nd_entry) * t->cap, 0, socket_id);
	if (!t->entries) {
		rte_free(t);
		return NULL;
	}

	struct rte_hash_parameters hp;
	memset(&hp, 0, sizeof(hp));
	hp.name = name;
	hp.entries = t->cap;
	hp.key_len = sizeof(struct rte_ipv6_addr);
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

void nd_table_free(struct nd_table *t) {
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

static struct nd_entry *get_or_create(struct nd_table *t, const struct rte_ipv6_addr *ip) {
	if (!t || !t->h || !t->entries || !ip) {
		return NULL;
	}
	int32_t pos = rte_hash_lookup(t->h, ip);
	if (pos >= 0) {
		return &t->entries[(uint32_t)pos];
	}
	int32_t add_pos = rte_hash_add_key(t->h, ip);
	if (add_pos < 0) {
		return NULL;
	}
	struct nd_entry *e = &t->entries[(uint32_t)add_pos];
	memset(e, 0, sizeof(*e));
	return e;
}

int nd_lookup(struct nd_table *t, const struct rte_ipv6_addr *ip, struct rte_ether_addr *mac_out) {
	if (!t || !t->h || !t->entries || !ip || !mac_out) {
		return -1;
	}
	int32_t pos = rte_hash_lookup(t->h, ip);
	if (pos < 0) {
		return -1;
	}
	struct nd_entry *e = &t->entries[(uint32_t)pos];
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

void nd_update(struct nd_table *t, const struct rte_ipv6_addr *ip, const struct rte_ether_addr *mac) {
	if (!t || !ip || !mac) {
		return;
	}
	struct nd_entry *e = get_or_create(t, ip);
	if (!e) {
		return;
	}
	e->mac = *mac;
	e->has_mac = 1;
	e->updated_tsc = rte_get_timer_cycles();
}

int nd_should_request(struct nd_table *t, const struct rte_ipv6_addr *ip) {
	if (!t || !ip) {
		return 0;
	}
	struct nd_entry *e = get_or_create(t, ip);
	if (!e) {
		return 0;
	}
	uint64_t now = rte_get_timer_cycles();
	if (e->requested_tsc == 0) {
		return 1;
	}
	return (now - e->requested_tsc) > t->req_interval_tsc;
}

void nd_mark_requested(struct nd_table *t, const struct rte_ipv6_addr *ip, uint64_t now_tsc) {
	if (!t || !ip) {
		return;
	}
	struct nd_entry *e = get_or_create(t, ip);
	if (!e) {
		return;
	}
	e->requested_tsc = now_tsc;
}

static int is_our_ip(const struct rte_ipv6_addr *ip, uint16_t port, const struct nd_ifcfg *ifcfgs, uint16_t nb_ports) {
	if (!ip || !ifcfgs || port >= nb_ports) {
		return 0;
	}
	if (!ifcfgs[port].configured) {
		return 0;
	}
	return rte_ipv6_addr_eq(ip, &ifcfgs[port].ip);
}

static const struct rte_ether_addr *nd_opt_lladdr(const uint8_t *opt, size_t opt_len, uint8_t want_type) {
	if (!opt || opt_len < 8) {
		return NULL;
	}
	uint8_t t = opt[0];
	uint8_t l = opt[1];
	size_t bytes = (size_t)l * 8;
	if (t != want_type || bytes < 8 || bytes > opt_len) {
		return NULL;
	}
	return (const struct rte_ether_addr *)(opt + 2);
}

int nd_process_packet(
	struct nd_table *t,
	struct rte_mbuf *m,
	uint16_t in_port,
	const struct nd_ifcfg *ifcfgs,
	const struct rte_ether_addr *port_macs,
	uint16_t nb_ports,
	uint16_t *tx_port_out
) {
	if (!m || !port_macs || !tx_port_out) {
		return 0;
	}
	struct rte_ether_hdr *eth = rte_pktmbuf_mtod(m, struct rte_ether_hdr *);
	uint16_t l3_off = sizeof(struct rte_ether_hdr);
	if (rte_pktmbuf_data_len(m) < l3_off + sizeof(struct rte_ipv6_hdr)) {
		return 0;
	}
	struct rte_ipv6_hdr *ip6 = (struct rte_ipv6_hdr *)((char *)eth + l3_off);
	if (ip6->proto != IPPROTO_ICMPV6 || ip6->hop_limits != 255) {
		return 0;
	}
	uint16_t l4_off = l3_off + sizeof(struct rte_ipv6_hdr);
	uint16_t plen = rte_be_to_cpu_16(ip6->payload_len);
	if (rte_pktmbuf_data_len(m) < l4_off + plen || plen < sizeof(struct rte_icmp_base_hdr)) {
		return 0;
	}
	struct rte_icmp_base_hdr *icmp = (struct rte_icmp_base_hdr *)((char *)eth + l4_off);
	uint8_t type = icmp->type;
	if (type != ICMPV6_TYPE_NS && type != ICMPV6_TYPE_NA) {
		return 0;
	}

	uint8_t *icmp_bytes = (uint8_t *)icmp;
	uint16_t icmp_len = plen;
	if (icmp_len < 24) {
		return 0;
	}
	struct rte_ipv6_addr target;
	memcpy(&target, icmp_bytes + 8, sizeof(target));
	const uint8_t *opt = icmp_bytes + 24;
	size_t opt_len = icmp_len - 24;

	const struct rte_ether_addr *opt_mac = NULL;
	if (type == ICMPV6_TYPE_NS) {
		opt_mac = nd_opt_lladdr(opt, opt_len, ND_OPT_SLLA);
	} else {
		opt_mac = nd_opt_lladdr(opt, opt_len, ND_OPT_TLLA);
	}
	if (opt_mac) {
		nd_update(t, &ip6->src_addr, opt_mac);
	} else {
		nd_update(t, &ip6->src_addr, &eth->src_addr);
	}

	if (type == ICMPV6_TYPE_NA) {
		if (opt_mac) {
			nd_update(t, &target, opt_mac);
		}
		return 0;
	}

	if (!is_our_ip(&target, in_port, ifcfgs, nb_ports)) {
		return 0;
	}

	struct rte_ether_addr dst = eth->src_addr;
	struct rte_ether_addr src = port_macs[in_port];

	icmp->type = ICMPV6_TYPE_NA;
	icmp->code = 0;
	icmp->checksum = 0;
	uint32_t flags = rte_cpu_to_be_32(0x60000000u);
	memcpy(icmp_bytes + 4, &flags, sizeof(flags));
	memcpy(icmp_bytes + 8, &target, sizeof(target));
	uint8_t *o = (uint8_t *)opt;
	if (opt_len >= 8) {
		o[0] = ND_OPT_TLLA;
		o[1] = 1;
		memcpy(o + 2, &src, sizeof(src));
		memset(o + 2 + sizeof(src), 0, 8 - 2 - sizeof(src));
		memset(o + 8, 0, opt_len - 8);
	} else {
		return 0;
	}

	ip6->dst_addr = ip6->src_addr;
	ip6->src_addr = ifcfgs[in_port].ip;
	ip6->payload_len = rte_cpu_to_be_16(icmp_len);
	ip6->proto = IPPROTO_ICMPV6;
	ip6->hop_limits = 255;

	rte_ether_addr_copy(&dst, &eth->dst_addr);
	rte_ether_addr_copy(&src, &eth->src_addr);

	icmp->checksum = icmpv6_cksum(ip6, icmp, icmp_len);
	*tx_port_out = in_port;
	return 1;
}

static void build_solicited_node_mcast(const struct rte_ipv6_addr *target, struct rte_ipv6_addr *dst) {
	static const uint8_t prefix[13] = { 0xff, 0x02, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0xff };
	memset(dst, 0, sizeof(*dst));
	memcpy(dst->a, prefix, sizeof(prefix));
	dst->a[13] = target->a[13];
	dst->a[14] = target->a[14];
	dst->a[15] = target->a[15];
}

static void build_solicited_node_mac(const struct rte_ipv6_addr *target, struct rte_ether_addr *mac) {
	mac->addr_bytes[0] = 0x33;
	mac->addr_bytes[1] = 0x33;
	mac->addr_bytes[2] = 0xff;
	mac->addr_bytes[3] = target->a[13];
	mac->addr_bytes[4] = target->a[14];
	mac->addr_bytes[5] = target->a[15];
}

struct rte_mbuf *nd_build_ns(
	struct rte_mempool *pool,
	const struct rte_ether_addr *src_mac,
	const struct rte_ipv6_addr *src_ip,
	const struct rte_ipv6_addr *target_ip
) {
	if (!pool || !src_mac || !src_ip || !target_ip) {
		return NULL;
	}
	struct rte_mbuf *m = rte_pktmbuf_alloc(pool);
	if (!m) {
		return NULL;
	}
	const uint16_t icmp_len = 32;
	const uint16_t pkt_size = sizeof(struct rte_ether_hdr) + sizeof(struct rte_ipv6_hdr) + icmp_len;
	char *data = rte_pktmbuf_append(m, pkt_size);
	if (!data) {
		rte_pktmbuf_free(m);
		return NULL;
	}
	memset(data, 0, pkt_size);

	struct rte_ether_hdr *eth = (struct rte_ether_hdr *)data;
	struct rte_ipv6_hdr *ip6 = (struct rte_ipv6_hdr *)(eth + 1);
	uint8_t *icmp = (uint8_t *)(ip6 + 1);

	struct rte_ether_addr dst_mac;
	build_solicited_node_mac(target_ip, &dst_mac);
	rte_ether_addr_copy(&dst_mac, &eth->dst_addr);
	rte_ether_addr_copy(src_mac, &eth->src_addr);
	eth->ether_type = rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV6);

	ip6->vtc_flow = rte_cpu_to_be_32(6u << 28);
	ip6->payload_len = rte_cpu_to_be_16(icmp_len);
	ip6->proto = IPPROTO_ICMPV6;
	ip6->hop_limits = 255;
	ip6->src_addr = *src_ip;
	build_solicited_node_mcast(target_ip, &ip6->dst_addr);

	struct rte_icmp_base_hdr *base = (struct rte_icmp_base_hdr *)icmp;
	base->type = ICMPV6_TYPE_NS;
	base->code = 0;
	base->checksum = 0;
	memcpy(icmp + 8, target_ip, sizeof(*target_ip));
	icmp[24] = ND_OPT_SLLA;
	icmp[25] = 1;
	memcpy(icmp + 26, src_mac, sizeof(*src_mac));

	base->checksum = icmpv6_cksum(ip6, icmp, icmp_len);
	return m;
}
