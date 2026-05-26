#ifndef DPDK_PF_ROUTE_IPC_H
#define DPDK_PF_ROUTE_IPC_H

#include <stdint.h>

#include <rte_atomic.h>

#define ROUTE_SHARED_NAME "route_shared_cfg"

#define ROUTE_MAX 1024

struct route4_item {
    uint32_t dst_ip;
    uint8_t depth;
    uint8_t reserved8[3];
    uint32_t next_hop_ip;
    uint16_t out_port;
    uint16_t reserved16;
};

struct route_shared_cfg {
    rte_atomic64_t version;
    uint32_t route_count;
    uint32_t reserved;
    struct route4_item routes[ROUTE_MAX];
};

#endif
