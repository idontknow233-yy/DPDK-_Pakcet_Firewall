#ifndef DPDK_PF_ACL_H
#define DPDK_PF_ACL_H

#include <stdbool.h>
#include <stdint.h>

#include <rte_ip.h>
#include <rte_rwlock.h>

struct acl_rule {
	uint32_t src_ip;
	uint32_t src_mask;
	uint32_t dst_ip;
	uint32_t dst_mask;
	uint16_t src_port_min;
	uint16_t src_port_max;
	uint16_t dst_port_min;
	uint16_t dst_port_max;
	uint8_t proto;
	uint8_t match_ports;
	uint8_t allow;
};

struct acl_ctx {
	struct acl_rule *rules;
	uint32_t count;
	uint32_t capacity;
	rte_rwlock_t lock;
};

int acl_init(struct acl_ctx *ctx, uint32_t capacity);
int acl_init_default(struct acl_ctx *ctx);
void acl_free(struct acl_ctx *ctx);
int acl_add_rule(struct acl_ctx *ctx, const struct acl_rule *rule);
int acl_delete_rule(struct acl_ctx *ctx, uint32_t index);
void acl_clear(struct acl_ctx *ctx);
uint32_t acl_count(const struct acl_ctx *ctx);
int acl_get_rule(const struct acl_ctx *ctx, uint32_t index, struct acl_rule *rule);
int acl_clone(struct acl_ctx *dst, const struct acl_ctx *src);
bool acl_check_ipv4(const struct acl_ctx *ctx, const struct rte_ipv4_hdr *ip, const void *l4_hdr);

#endif
