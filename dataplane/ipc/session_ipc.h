#ifndef DPDK_PF_SESSION_IPC_H
#define DPDK_PF_SESSION_IPC_H

#include <stdint.h>

#include <rte_atomic.h>

#include "session/session.h"

#define SESSION_SHARED_NAME "session_shared_cfg"

#define SESSION_MAX_EXPORT 4096

struct session_shared_cfg {
	rte_atomic64_t version;
	uint32_t count;
	struct session_key keys[SESSION_MAX_EXPORT];
	struct session_entry entries[SESSION_MAX_EXPORT];
};

#endif
