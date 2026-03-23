#include "mock_dpdk.h"
#include <stdlib.h>
#include <string.h>

uint16_t rte_be_to_cpu_16(uint16_t x) {
    return ((x & 0xFF) << 8) | ((x >> 8) & 0xFF);
}

uint16_t rte_cpu_to_be_16(uint16_t x) {
    return rte_be_to_cpu_16(x);
}

uint32_t rte_be_to_cpu_32(uint32_t x) {
    return ((x & 0xFF) << 24) |
           ((x & 0xFF00) << 8) |
           ((x & 0xFF0000) >> 8) |
           ((x & 0xFF000000) >> 24);
}

uint32_t rte_cpu_to_be_32(uint32_t x) {
    return rte_be_to_cpu_32(x);
}

uint64_t rte_be_to_cpu_64(uint64_t x) {
    return ((x & 0xFFULL) << 56) |
           ((x & 0xFF00ULL) << 40) |
           ((x & 0xFF0000ULL) << 24) |
           ((x & 0xFF000000ULL) << 8) |
           ((x & 0xFF00000000ULL) >> 8) |
           ((x & 0xFF0000000000ULL) >> 24) |
           ((x & 0xFF000000000000ULL) >> 40) |
           ((x & 0xFF00000000000000ULL) >> 56);
}

void *rte_zmalloc(const char *type, size_t size, unsigned align) {
    (void)type;
    (void)align;
    void *ptr = calloc(1, size);
    return ptr;
}

void *rte_malloc(const char *type, size_t size, unsigned align) {
    (void)type;
    (void)align;
    return malloc(size);
}

void rte_free(void *ptr) {
    free(ptr);
}

uint16_t rte_ipv4_cksum(const rte_ipv4_hdr *hdr) {
    uint32_t sum = 0;
    uint16_t *w = (uint16_t *)hdr;
    int len = (hdr->version_ihl & 0x0F) * 4;
    
    for (int i = 0; i < len; i += 2) {
        sum += w[i/2];
    }
    
    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }
    
    return ~sum;
}

uint16_t rte_ipv6_phdr_cksum(const rte_ipv6_hdr *hdr, uint32_t flip) {
    (void)hdr;
    (void)flip;
    return 0;
}

void rte_rwlock_init(rte_rwlock_t *rwl) {
    (void)rwl;
}

void rte_rwlock_read_lock(rte_rwlock_t *rwl) {
    (void)rwl;
}

void rte_rwlock_read_unlock(rte_rwlock_t *rwl) {
    (void)rwl;
}

void rte_rwlock_write_lock(rte_rwlock_t *rwl) {
    (void)rwl;
}

void rte_rwlock_write_unlock(rte_rwlock_t *rwl) {
    (void)rwl;
}

void rte_spinlock_init(rte_spinlock_t *sl) {
    (void)sl;
}

void rte_spinlock_lock(rte_spinlock_t *sl) {
    (void)sl;
}

void rte_spinlock_unlock(rte_spinlock_t *sl) {
    (void)sl;
}

void rte_atomic32_init(rte_atomic32_t *v) {
    v->cnt = 0;
}

int32_t rte_atomic32_read(rte_atomic32_t *v) {
    return v->cnt;
}

void rte_atomic32_set(rte_atomic32_t *v, int32_t new_value) {
    v->cnt = new_value;
}

void rte_atomic32_inc(rte_atomic32_t *v) {
    v->cnt++;
}

int32_t rte_atomic32_add_return(rte_atomic32_t *v, int32_t delta) {
    return v->cnt += delta;
}

void rte_atomic64_init(rte_atomic64_t *v) {
    v->cnt = 0;
}

uint64_t rte_atomic64_read(rte_atomic64_t *v) {
    return v->cnt;
}

void rte_atomic64_set(rte_atomic64_t *v, uint64_t new_value) {
    v->cnt = new_value;
}

void rte_atomic64_inc(rte_atomic64_t *v) {
    v->cnt++;
}

uint64_t rte_atomic64_add_return(rte_atomic64_t *v, uint64_t delta) {
    return v->cnt += delta;
}

void rte_wmb(void) {
}

void rte_rmb(void) {
}

uint64_t rte_get_timer_cycles(void) {
    return 1000;
}

uint64_t rte_get_timer_hz(void) {
    return 1000000000;
}

int rte_socket_id(void) {
    return 0;
}

void rte_ipv6_addr_mask(rte_ipv6_addr *dst, const rte_ipv6_addr *mask) {
    for (int i = 0; i < 16; i++) {
        dst->addr[i] &= mask->addr[i];
    }
}

int rte_ipv6_addr_cmp(const rte_ipv6_addr *a, const rte_ipv6_addr *b) {
    return memcmp(a->addr, b->addr, 16);
}
