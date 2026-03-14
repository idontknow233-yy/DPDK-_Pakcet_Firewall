#ifndef DPDK_PF_ACL6_H
#define DPDK_PF_ACL6_H

#include <stdbool.h>
#include <stdint.h>

#include <rte_ip6.h>
#include <rte_rwlock.h>

struct acl6_rule {
	struct rte_ipv6_addr src_ip;
	uint8_t src_depth;
	struct rte_ipv6_addr dst_ip;
	uint8_t dst_depth;
	uint16_t src_port_min;
	uint16_t src_port_max;
	uint16_t dst_port_min;
	uint16_t dst_port_max;
	uint8_t proto;
	uint8_t match_ports;
	uint8_t allow;
};

struct acl6_ctx {
	struct acl6_rule *rules;
	uint32_t count;
	uint32_t capacity;
	rte_rwlock_t lock;
};

int acl6_init(struct acl6_ctx *ctx, uint32_t capacity);
void acl6_free(struct acl6_ctx *ctx);
int acl6_add_rule(struct acl6_ctx *ctx, const struct acl6_rule *rule);
int acl6_delete_rule(struct acl6_ctx *ctx, uint32_t index);
void acl6_clear(struct acl6_ctx *ctx);
uint32_t acl6_count(const struct acl6_ctx *ctx);
int acl6_get_rule(const struct acl6_ctx *ctx, uint32_t index, struct acl6_rule *rule);
int acl6_clone(struct acl6_ctx *dst, const struct acl6_ctx *src);
bool acl6_check_ipv6(const struct acl6_ctx *ctx, const struct rte_ipv6_hdr *ip6, const void *l4_hdr, uint32_t *deny_rule_index);

#endif
