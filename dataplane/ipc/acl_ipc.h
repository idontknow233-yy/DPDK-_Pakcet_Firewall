#ifndef DPDK_PF_ACL_IPC_H
#define DPDK_PF_ACL_IPC_H

#include <stdint.h>

#include <rte_atomic.h>

#include "acl/acl.h"

#define ACL_CMD_RING_NAME "acl_cmd_ring"
#define ACL_RESP_RING_NAME "acl_resp_ring"
#define ACL_SHARED_CFG_NAME "acl_shared_cfg"

#define ACL_MAX_RULES 256
#define ACL_CMD_RING_SIZE 1024
#define ACL_RESP_RING_SIZE 1024

enum acl_cmd_type {
	ACL_CMD_ADD = 1,
	ACL_CMD_DEL,
	ACL_CMD_CLEAR,
	ACL_CMD_LIST,
};

struct acl_cmd_msg {
	uint32_t type;
	uint32_t seq;
	struct acl_rule rule;
	uint32_t index;
};

#define ACL_RESP_FLAG_LIST_ITEM 0x1

struct acl_cmd_resp {
	uint32_t seq;
	int32_t status;
	uint32_t count;
	uint64_t version;
	struct acl_rule rule;
	uint32_t rule_index;
	uint32_t flags;
};

struct acl_shared_cfg {
	rte_atomic64_t version;
	uint32_t count;
	struct acl_rule rules[ACL_MAX_RULES];
};

#endif
