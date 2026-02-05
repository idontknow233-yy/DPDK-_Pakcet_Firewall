#include "acl.h"

#include <string.h>

#include <netinet/in.h>

#include <rte_byteorder.h>
#include <rte_malloc.h>
#include <rte_tcp.h>
#include <rte_udp.h>

static bool acl_match_ports(const struct acl_rule *rule, uint8_t proto, uint16_t src_port, uint16_t dst_port) {
	if (!rule->match_ports) {
		return true;
	}
	if (rule->proto != 0 && rule->proto != proto) {
		return false;
	}
	if (src_port < rule->src_port_min || src_port > rule->src_port_max) {
		return false;
	}
	if (dst_port < rule->dst_port_min || dst_port > rule->dst_port_max) {
		return false;
	}
	return true;
}

int acl_init(struct acl_ctx *ctx, uint32_t capacity) {
	if (!ctx || capacity == 0) {
		return -1;
	}
	ctx->rules = rte_zmalloc("acl_rules", sizeof(struct acl_rule) * capacity, 0);
	if (!ctx->rules) {
		return -1;
	}
	ctx->count = 0;
	ctx->capacity = capacity;
	rte_rwlock_init(&ctx->lock);
	return 0;
}

int acl_add_rule(struct acl_ctx *ctx, const struct acl_rule *rule) {
	if (!ctx || !ctx->rules || !rule) {
		return -1;
	}
	rte_rwlock_write_lock(&ctx->lock);
	if (ctx->count >= ctx->capacity) {
		rte_rwlock_write_unlock(&ctx->lock);
		return -1;
	}
	ctx->rules[ctx->count++] = *rule;
	rte_rwlock_write_unlock(&ctx->lock);
	return 0;
}

int acl_delete_rule(struct acl_ctx *ctx, uint32_t index) {
	if (!ctx || !ctx->rules) {
		return -1;
	}
	rte_rwlock_write_lock(&ctx->lock);
	if (index >= ctx->count) {
		rte_rwlock_write_unlock(&ctx->lock);
		return -1;
	}
	if (index + 1 < ctx->count) {
		memmove(&ctx->rules[index], &ctx->rules[index + 1],
			sizeof(struct acl_rule) * (ctx->count - index - 1));
	}
	ctx->count--;
	rte_rwlock_write_unlock(&ctx->lock);
	return 0;
}

void acl_clear(struct acl_ctx *ctx) {
	if (!ctx || !ctx->rules) {
		return;
	}
	rte_rwlock_write_lock(&ctx->lock);
	ctx->count = 0;
	rte_rwlock_write_unlock(&ctx->lock);
}

uint32_t acl_count(const struct acl_ctx *ctx) {
	if (!ctx || !ctx->rules) {
		return 0;
	}
	rte_rwlock_read_lock((rte_rwlock_t *)&ctx->lock);
	uint32_t count = ctx->count;
	rte_rwlock_read_unlock((rte_rwlock_t *)&ctx->lock);
	return count;
}

int acl_get_rule(const struct acl_ctx *ctx, uint32_t index, struct acl_rule *rule) {
	if (!ctx || !ctx->rules || !rule) {
		return -1;
	}
	rte_rwlock_read_lock((rte_rwlock_t *)&ctx->lock);
	if (index >= ctx->count) {
		rte_rwlock_read_unlock((rte_rwlock_t *)&ctx->lock);
		return -1;
	}
	*rule = ctx->rules[index];
	rte_rwlock_read_unlock((rte_rwlock_t *)&ctx->lock);
	return 0;
}

int acl_init_default(struct acl_ctx *ctx) {
	if (acl_init(ctx, 16) != 0) {
		return -1;
	}
	struct acl_rule deny_private = {
		.src_ip = rte_be_to_cpu_32(RTE_IPV4(10, 0, 0, 0)),
		.src_mask = rte_be_to_cpu_32(RTE_IPV4(255, 0, 0, 0)),
		.dst_ip = 0,
		.dst_mask = 0,
		.src_port_min = 0,
		.src_port_max = 0,
		.dst_port_min = 0,
		.dst_port_max = 0,
		.proto = 0,
		.match_ports = 0,
		.allow = 0,
	};
	struct acl_rule allow_all = {
		.src_ip = 0,
		.src_mask = 0,
		.dst_ip = 0,
		.dst_mask = 0,
		.src_port_min = 0,
		.src_port_max = 0,
		.dst_port_min = 0,
		.dst_port_max = 0,
		.proto = 0,
		.match_ports = 0,
		.allow = 1,
	};
	if (acl_add_rule(ctx, &deny_private) != 0) {
		return -1;
	}
	if (acl_add_rule(ctx, &allow_all) != 0) {
		return -1;
	}
	return 0;
}

void acl_free(struct acl_ctx *ctx) {
	if (!ctx) {
		return;
	}
	if (ctx->rules) {
		rte_free(ctx->rules);
		ctx->rules = NULL;
	}
	ctx->count = 0;
	ctx->capacity = 0;
}

bool acl_check_ipv4(const struct acl_ctx *ctx, const struct rte_ipv4_hdr *ip, const void *l4_hdr) {
	if (!ctx || !ctx->rules || !ip) {
		return true;
	}
	uint32_t src = rte_be_to_cpu_32(ip->src_addr);
	uint32_t dst = rte_be_to_cpu_32(ip->dst_addr);
	uint8_t proto = ip->next_proto_id;
	uint16_t src_port = 0;
	uint16_t dst_port = 0;

	if (l4_hdr && (proto == IPPROTO_TCP || proto == IPPROTO_UDP)) {
		if (proto == IPPROTO_TCP) {
			const struct rte_tcp_hdr *tcp = (const struct rte_tcp_hdr *)l4_hdr;
			src_port = rte_be_to_cpu_16(tcp->src_port);
			dst_port = rte_be_to_cpu_16(tcp->dst_port);
		} else {
			const struct rte_udp_hdr *udp = (const struct rte_udp_hdr *)l4_hdr;
			src_port = rte_be_to_cpu_16(udp->src_port);
			dst_port = rte_be_to_cpu_16(udp->dst_port);
		}
	}

	rte_rwlock_read_lock((rte_rwlock_t *)&ctx->lock);
	for (uint32_t i = 0; i < ctx->count; i++) {
		const struct acl_rule *rule = &ctx->rules[i];
		if (rule->src_mask && ((src & rule->src_mask) != (rule->src_ip & rule->src_mask))) {
			continue;
		}
		if (rule->dst_mask && ((dst & rule->dst_mask) != (rule->dst_ip & rule->dst_mask))) {
			continue;
		}
		if (!acl_match_ports(rule, proto, src_port, dst_port)) {
			continue;
		}
		bool allow = rule->allow != 0;
		rte_rwlock_read_unlock((rte_rwlock_t *)&ctx->lock);
		return allow;
	}
	rte_rwlock_read_unlock((rte_rwlock_t *)&ctx->lock);

	return true;
}
