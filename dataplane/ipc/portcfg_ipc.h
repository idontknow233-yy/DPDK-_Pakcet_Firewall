#ifndef DPDK_PF_PORTCFG_IPC_H
#define DPDK_PF_PORTCFG_IPC_H

#include <stdint.h>

#include <rte_atomic.h>

#define PORTCFG_SHARED_NAME "portcfg_shared_cfg"

#define PORTCFG_MAX_PORTS 32

struct portcfg_item {
    uint32_t ip;
    uint32_t mask;
    uint8_t configured;
    uint8_t reserved[3];
};

struct portcfg_shared_cfg {
    rte_atomic64_t version;
    struct portcfg_item ports[PORTCFG_MAX_PORTS];
};

#endif
