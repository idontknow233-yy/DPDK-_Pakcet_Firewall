#include "acl6.h"

#include <string.h>

#include <netinet/in.h>

#include <rte_byteorder.h>
#include <rte_malloc.h>
#include <rte_tcp.h>
#include <rte_udp.h>

static bool acl6_match_ports(const struct acl6_rule *rule, uint8_t proto, uint16_t src_port, uint16_t dst_port) {
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

int acl6_init(struct acl6_ctx *ctx, uint32_t capacity) {
	if (!ctx || capacity == 0) {
		return -1;
	}
	ctx->rules = rte_zmalloc("acl6_rules", sizeof(struct acl6_rule) * capacity, 0);
	if (!ctx->rules) {
		return -1;
	}
	ctx->count = 0;
	ctx->capacity = capacity;
	rte_rwlock_init(&ctx->lock);
	return 0;
}

int acl6_add_rule(struct acl6_ctx *ctx, const struct acl6_rule *rule) {
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

int acl6_delete_rule(struct acl6_ctx *ctx, uint32_t index) {
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
			sizeof(struct acl6_rule) * (ctx->count - index - 1));
	}
	ctx->count--;
	rte_rwlock_write_unlock(&ctx->lock);
	return 0;
}

void acl6_clear(struct acl6_ctx *ctx) {
	if (!ctx || !ctx->rules) {
		return;
	}
	rte_rwlock_write_lock(&ctx->lock);
	ctx->count = 0;
	rte_rwlock_write_unlock(&ctx->lock);
}

uint32_t acl6_count(const struct acl6_ctx *ctx) {
	if (!ctx || !ctx->rules) {
		return 0;
	}
	rte_rwlock_read_lock((rte_rwlock_t *)&ctx->lock);
	uint32_t count = ctx->count;
	rte_rwlock_read_unlock((rte_rwlock_t *)&ctx->lock);
	return count;
}

int acl6_get_rule(const struct acl6_ctx *ctx, uint32_t index, struct acl6_rule *rule) {
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

int acl6_clone(struct acl6_ctx *dst, const struct acl6_ctx *src) {
	if (!dst || !src) {
		return -1;
	}
	if (!src->rules) {
		return -1;
	}
	rte_rwlock_read_lock((rte_rwlock_t *)&src->lock);
	uint32_t capacity = src->capacity;
	rte_rwlock_read_unlock((rte_rwlock_t *)&src->lock);

	rte_rwlock_write_lock(&dst->lock);
	if (!dst->rules || dst->capacity < capacity) {
		if (dst->rules) {
			rte_free(dst->rules);
		}
		dst->rules = rte_zmalloc("acl6_rules", sizeof(struct acl6_rule) * capacity, 0);
		if (!dst->rules) {
			dst->count = 0;
			dst->capacity = 0;
			rte_rwlock_write_unlock(&dst->lock);
			return -1;
		}
		dst->capacity = capacity;
	}
	rte_rwlock_read_lock((rte_rwlock_t *)&src->lock);
	dst->count = src->count;
	memcpy(dst->rules, src->rules, sizeof(struct acl6_rule) * src->count);
	rte_rwlock_read_unlock((rte_rwlock_t *)&src->lock);
	rte_rwlock_write_unlock(&dst->lock);
	return 0;
}

void acl6_free(struct acl6_ctx *ctx) {
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

bool acl6_check_ipv6(const struct acl6_ctx *ctx, const struct rte_ipv6_hdr *ip6, const void *l4_hdr, uint32_t *deny_rule_index) {
	if (!ctx || !ctx->rules || !ip6) {
		if (deny_rule_index) {
			*deny_rule_index = UINT32_MAX;
		}
		return true;
	}
	uint8_t proto = ip6->proto;
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
		const struct acl6_rule *rule = &ctx->rules[i];
		if (rule->src_depth != 0 && !rte_ipv6_addr_eq_prefix(&ip6->src_addr, &rule->src_ip, rule->src_depth)) {
			continue;
		}
		if (rule->dst_depth != 0 && !rte_ipv6_addr_eq_prefix(&ip6->dst_addr, &rule->dst_ip, rule->dst_depth)) {
			continue;
		}
		if (!acl6_match_ports(rule, proto, src_port, dst_port)) {
			continue;
		}
		if (!rule->allow) {
			if (deny_rule_index) {
				*deny_rule_index = i;
			}
			rte_rwlock_read_unlock((rte_rwlock_t *)&ctx->lock);
			return false;
		}
	}
	rte_rwlock_read_unlock((rte_rwlock_t *)&ctx->lock);
	if (deny_rule_index) {
		*deny_rule_index = UINT32_MAX;
	}
	return true;
}

