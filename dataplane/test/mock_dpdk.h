#pragma once

#include <stdint.h>
#include <string.h>
#include <stdbool.h>

#define RTE_MAX_LCORE 64
#define RTE_MAX_ETHPORTS 16
#define RTE_ETHER_ADDR_LEN 6
#define RTE_LCORE_MAX 64

#define IPPROTO_TCP 6
#define IPPROTO_UDP 17
#define IPPROTO_ICMP 1
#define IPPROTO_ICMPV6 58

#define RTE_IPV4(a, b, c, d) ((uint32_t)(((a) << 24) | ((b) << 16) | ((c) << 8) | (d)))
#define RTE_IPV4_MASK(a, b, c, d) RTE_IPV4(a, b, c, d)

#define RTE_ETHER_TYPE_IPV4 0x0800
#define RTE_ETHER_TYPE_IPV6 0x86DD
#define RTE_ETHER_TYPE_ARP 0x0806

typedef struct {
    uint8_t addr_bytes[RTE_ETHER_ADDR_LEN];
} __attribute__((aligned(1))) rte_ether_addr;

typedef struct {
    uint16_t src_port;
    uint16_t dst_port;
    uint8_t tcp_flags;
    uint16_t rx_win;
    uint16_t crc;
} __attribute__((aligned(1))) rte_tcp_hdr;

typedef struct {
    uint16_t src_port;
    uint16_t dst_port;
    uint16_t dgram_len;
    uint16_t crc;
} __attribute__((aligned(1))) rte_udp_hdr;

typedef struct {
    uint32_t src_addr;
    uint32_t dst_addr;
    uint16_t src_port;
    uint16_t dst_port;
    uint8_t next_proto_id;
    uint8_t version_ihl;
    uint8_t time_to_live;
    uint16_t hdr_checksum;
} __attribute__((aligned(1))) rte_ipv4_hdr;

typedef struct {
    uint8_t src_addr[16];
    uint8_t dst_addr[16];
    uint32_t payload_length;
    uint8_t next_proto_id;
} __attribute__((aligned(1))) rte_ipv6_hdr;

typedef struct {
    uint16_t ether_type;
    rte_ether_addr dst_addr;
    rte_ether_addr src_addr;
} __attribute__((aligned(1))) rte_ether_hdr;

typedef struct {
    uint8_t sip[4];
    uint8_t dip[4];
    uint8_t zero;
    uint8_t proto;
    uint16_t len;
} __attribute__((aligned(1))) rte_arp_ipv4;

typedef struct {
    uint16_t arp_hrd;
    uint16_t arp_pro;
    uint8_t arp_hln;
    uint8_t arp_pln;
    uint16_t arp_op;
    rte_ether_addr arp_sha;
    uint8_t arp_sip[4];
    rte_ether_addr arp_tha;
    uint8_t arp_tip[4];
} __attribute__((aligned(1))) rte_arp_hdr;

uint16_t rte_be_to_cpu_16(uint16_t x);
uint16_t rte_cpu_to_be_16(uint16_t x);
uint32_t rte_be_to_cpu_32(uint32_t x);
uint32_t rte_cpu_to_be_32(uint32_t x);
uint64_t rte_be_to_cpu_64(uint64_t x);

void *rte_zmalloc(const char *type, size_t size, unsigned align);
void *rte_malloc(const char *type, size_t size, unsigned align);
void rte_free(void *ptr);

uint16_t rte_ipv4_cksum(const rte_ipv4_hdr *hdr);
uint16_t rte_ipv6_phdr_cksum(const rte_ipv6_hdr *hdr, uint32_t flip);

typedef struct {} rte_rwlock_t;
void rte_rwlock_init(rte_rwlock_t *rwl);
void rte_rwlock_read_lock(rte_rwlock_t *rwl);
void rte_rwlock_read_unlock(rte_rwlock_t *rwl);
void rte_rwlock_write_lock(rte_rwlock_t *rwl);
void rte_rwlock_write_unlock(rte_rwlock_t *rwl);

typedef struct {} rte_spinlock_t;
void rte_spinlock_init(rte_spinlock_t *sl);
void rte_spinlock_lock(rte_spinlock_t *sl);
void rte_spinlock_unlock(rte_spinlock_t *sl);

typedef struct { volatile int32_t cnt; } rte_atomic32_t;
typedef struct { volatile uint32_t cnt; } rte_atomic32_uint32_t;
typedef struct { volatile uint64_t cnt; } rte_atomic64_t;

void rte_atomic32_init(rte_atomic32_t *v);
int32_t rte_atomic32_read(rte_atomic32_t *v);
void rte_atomic32_set(rte_atomic32_t *v, int32_t new_value);
void rte_atomic32_inc(rte_atomic32_t *v);
int32_t rte_atomic32_add_return(rte_atomic32_t *v, int32_t delta);

void rte_atomic64_init(rte_atomic64_t *v);
uint64_t rte_atomic64_read(rte_atomic64_t *v);
void rte_atomic64_set(rte_atomic64_t *v, uint64_t new_value);
void rte_atomic64_inc(rte_atomic64_t *v);
uint64_t rte_atomic64_add_return(rte_atomic64_t *v, uint64_t delta);

void rte_wmb(void);
void rte_rmb(void);

uint64_t rte_get_timer_cycles(void);
uint64_t rte_get_timer_hz(void);

int rte_socket_id(void);

#define RTE_IPV6_ADDR_UNSPECIFIED { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }
#define RTE_IPV6_ADDR_UNSPEC { RTE_IPV6_ADDR_UNSPECIFIED }

typedef struct {
    uint8_t addr[16];
} __attribute__((aligned(1))) rte_ipv6_addr;

void rte_ipv6_addr_mask(rte_ipv6_addr *dst, const rte_ipv6_addr *mask);
int rte_ipv6_addr_cmp(const rte_ipv6_addr *a, const rte_ipv6_addr *b);
