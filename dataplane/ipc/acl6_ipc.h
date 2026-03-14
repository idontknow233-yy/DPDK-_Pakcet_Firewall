#ifndef DPDK_PF_ACL6_IPC_H
#define DPDK_PF_ACL6_IPC_H

#include <stdint.h>

#include <rte_atomic.h>

#include "acl/acl6.h"

#define ACL6_CMD_RING_NAME "acl6_cmd_ring"
#define ACL6_RESP_RING_NAME "acl6_resp_ring"
#define ACL6_SHARED_CFG_NAME "acl6_shared_cfg"

#define ACL6_MAX_RULES 256
#define ACL6_CMD_RING_SIZE 1024
#define ACL6_RESP_RING_SIZE 1024

enum acl6_cmd_type {
	ACL6_CMD_ADD = 1,
	ACL6_CMD_DEL,
	ACL6_CMD_CLEAR,
	ACL6_CMD_LIST,
};

struct acl6_cmd_msg {
	uint32_t type;
	uint32_t seq;
	struct acl6_rule rule;
	uint32_t index;
};

struct acl6_cmd_resp {
	uint32_t seq;
	int32_t status;
	uint32_t count;
	uint64_t version;
	struct acl6_rule rule;
	uint32_t rule_index;
	uint32_t flags;
};

struct acl6_shared_cfg {
	rte_atomic64_t version;
	uint32_t count;
	struct acl6_rule rules[ACL6_MAX_RULES];
};

#endif
