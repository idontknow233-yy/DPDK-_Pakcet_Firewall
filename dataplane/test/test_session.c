#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define IPPROTO_TCP 6
#define IPPROTO_UDP 17
#define RTE_IPV4(a, b, c, d) ((uint32_t)(((a) << 24) | ((b) << 16) | ((c) << 8) | (d)))

typedef struct { volatile uint64_t cnt; } rte_atomic64_t;

void rte_atomic64_init(rte_atomic64_t *v) { v->cnt = 0; }
uint64_t rte_atomic64_read(rte_atomic64_t *v) { return v->cnt; }
void rte_atomic64_inc(rte_atomic64_t *v) { v->cnt++; }
void rte_wmb(void) {}

struct session_key {
    uint32_t src_ip;
    uint32_t dst_ip;
    uint16_t src_port;
    uint16_t dst_port;
    uint8_t proto;
};

struct session_entry {
    uint64_t last_seen_tsc;
    uint64_t packets;
    uint64_t bytes;
};

struct session_bucket {
    struct session_key key;
    struct session_entry entry;
    uint8_t valid;
};

struct session_table {
    struct session_bucket *buckets;
    uint32_t capacity;
    uint32_t count;
};

int session_table_init(struct session_table *tbl, const char *name, uint32_t capacity, int socket_id) {
    (void)name; (void)socket_id;
    if (!tbl || capacity == 0) return -1;
    tbl->buckets = calloc(capacity, sizeof(struct session_bucket));
    if (!tbl->buckets) return -1;
    tbl->capacity = capacity;
    tbl->count = 0;
    return 0;
}

void session_table_free(struct session_table *tbl) {
    if (!tbl) return;
    if (tbl->buckets) free(tbl->buckets);
    tbl->buckets = NULL;
    tbl->capacity = 0;
    tbl->count = 0;
}

static uint32_t session_hash(const struct session_key *key) {
    uint32_t h = key->src_ip ^ key->dst_ip ^ key->src_port ^ key->dst_port ^ key->proto;
    return h;
}

struct session_entry *session_lookup(struct session_table *tbl, const struct session_key *key) {
    if (!tbl || !tbl->buckets || !key) return NULL;
    uint32_t idx = session_hash(key) % tbl->capacity;
    struct session_bucket *b = &tbl->buckets[idx];
    if (b->valid && b->key.src_ip == key->src_ip && b->key.dst_ip == key->dst_ip &&
        b->key.src_port == key->src_port && b->key.dst_port == key->dst_port && b->key.proto == key->proto) {
        return &b->entry;
    }
    return NULL;
}

int session_track(struct session_table *tbl, const struct session_key *key, uint32_t pkt_size, uint64_t now_tsc, void *ctx) {
    (void)ctx;
    if (!tbl || !tbl->buckets || !key) return -1;
    uint32_t idx = session_hash(key) % tbl->capacity;
    struct session_bucket *b = &tbl->buckets[idx];
    if (!b->valid) {
        b->key = *key;
        b->entry.last_seen_tsc = now_tsc;
        b->entry.packets = 0;
        b->entry.bytes = 0;
        b->valid = 1;
        tbl->count++;
    }
    b->entry.last_seen_tsc = now_tsc;
    b->entry.packets++;
    b->entry.bytes += pkt_size;
    return 0;
}

void session_soft_expire(struct session_table *tbl, uint64_t now_tsc, uint64_t timeout_tsc) {
    if (!tbl || !tbl->buckets) return;
    for (uint32_t i = 0; i < tbl->capacity; i++) {
        struct session_bucket *b = &tbl->buckets[i];
        if (b->valid && (now_tsc - b->entry.last_seen_tsc) > timeout_tsc) {
            b->valid = 0;
            tbl->count--;
        }
    }
}

static int tests_run = 0;
static int tests_passed = 0;

#define ck_assert_int_eq(a, b) do { \
    tests_run++; \
    if ((int)(a) == (int)(b)) { \
        tests_passed++; \
        printf("  PASS: %s == %s (%d == %d)\n", #a, #b, (int)(a), (int)(b)); \
    } else { \
        printf("  FAIL: %s != %s (%d != %d)\n", #a, #b, (int)(a), (int)(b)); \
    } \
} while(0)

#define ck_assert_ptr_ne(a, b) do { \
    tests_run++; \
    if ((a) != (b)) { \
        tests_passed++; \
        printf("  PASS: %s != %s\n", #a, #b); \
    } else { \
        printf("  FAIL: %s == %s\n", #a, #b); \
    } \
} while(0)

#define ck_assert(expr) do { \
    tests_run++; \
    if (expr) { \
        tests_passed++; \
        printf("  PASS: %s\n", #expr); \
    } else { \
        printf("  FAIL: %s\n", #expr); \
    } \
} while(0)

void test_session_create(void) {
    printf("\n=== test_session_create ===\n");
    struct session_table tbl;
    
    ck_assert_int_eq(session_table_init(&tbl, "test", 1024, 0), 0);
    ck_assert_ptr_ne(tbl.buckets, NULL);
    ck_assert_int_eq(tbl.count, 0);
    
    struct session_key key = {
        .src_ip = RTE_IPV4(192, 168, 1, 100),
        .dst_ip = RTE_IPV4(10, 0, 0, 1),
        .src_port = 12345,
        .dst_port = 80,
        .proto = IPPROTO_TCP,
    };
    
    ck_assert_int_eq(session_track(&tbl, &key, 64, 1000, NULL), 0);
    ck_assert_int_eq(tbl.count, 1);
    
    session_table_free(&tbl);
}

void test_session_lookup(void) {
    printf("\n=== test_session_lookup ===\n");
    struct session_table tbl;
    
    session_table_init(&tbl, "test", 1024, 0);
    
    struct session_key key = {
        .src_ip = RTE_IPV4(192, 168, 1, 100),
        .dst_ip = RTE_IPV4(10, 0, 0, 1),
        .src_port = 12345,
        .dst_port = 80,
        .proto = IPPROTO_TCP,
    };
    
    session_track(&tbl, &key, 64, 1000, NULL);
    
    struct session_entry *found = session_lookup(&tbl, &key);
    ck_assert_ptr_ne(found, NULL);
    if (found) {
        ck_assert_int_eq(found->packets, 1);
        ck_assert_int_eq(found->bytes, 64);
    }
    
    struct session_key key_not_exist = {
        .src_ip = RTE_IPV4(192, 168, 1, 200),
        .dst_ip = RTE_IPV4(10, 0, 0, 1),
        .src_port = 12345,
        .dst_port = 80,
        .proto = IPPROTO_TCP,
    };
    ck_assert_ptr_ne(session_lookup(&tbl, &key_not_exist), NULL);
    
    session_table_free(&tbl);
}

void test_session_update(void) {
    printf("\n=== test_session_update ===\n");
    struct session_table tbl;
    
    session_table_init(&tbl, "test", 1024, 0);
    
    struct session_key key = {
        .src_ip = RTE_IPV4(192, 168, 1, 100),
        .dst_ip = RTE_IPV4(10, 0, 0, 1),
        .src_port = 12345,
        .dst_port = 80,
        .proto = IPPROTO_TCP,
    };
    
    session_track(&tbl, &key, 64, 1000, NULL);
    struct session_entry *found = session_lookup(&tbl, &key);
    ck_assert_int_eq(found->packets, 1);
    
    session_track(&tbl, &key, 128, 2000, NULL);
    found = session_lookup(&tbl, &key);
    ck_assert_int_eq(found->packets, 2);
    ck_assert_int_eq(found->bytes, 192);
    
    session_table_free(&tbl);
}

void test_session_expiration(void) {
    printf("\n=== test_session_expiration ===\n");
    struct session_table tbl;
    
    session_table_init(&tbl, "test", 1024, 0);
    
    struct session_key key = {
        .src_ip = RTE_IPV4(192, 168, 1, 100),
        .dst_ip = RTE_IPV4(10, 0, 0, 1),
        .src_port = 12345,
        .dst_port = 80,
        .proto = IPPROTO_TCP,
    };
    
    session_track(&tbl, &key, 64, 1000, NULL);
    ck_assert_int_eq(tbl.count, 1);
    
    session_soft_expire(&tbl, 10000, 100);
    ck_assert_int_eq(tbl.count, 0);
    
    session_table_free(&tbl);
}

void test_session_no_expire(void) {
    printf("\n=== test_session_no_expire ===\n");
    struct session_table tbl;
    
    session_table_init(&tbl, "test", 1024, 0);
    
    struct session_key key = {
        .src_ip = RTE_IPV4(192, 168, 1, 100),
        .dst_ip = RTE_IPV4(10, 0, 0, 1),
        .src_port = 12345,
        .dst_port = 80,
        .proto = IPPROTO_TCP,
    };
    
    session_track(&tbl, &key, 64, 1000, NULL);
    ck_assert_int_eq(tbl.count, 1);
    
    session_soft_expire(&tbl, 1050, 100);
    ck_assert_int_eq(tbl.count, 1);
    
    session_table_free(&tbl);
}

int main(void) {
    printf("=========================================\n");
    printf("Session Unit Tests (Standalone)\n");
    printf("=========================================\n");
    
    test_session_create();
    test_session_lookup();
    test_session_update();
    test_session_expiration();
    test_session_no_expire();
    
    printf("\n=========================================\n");
    printf("Results: %d/%d tests passed\n", tests_passed, tests_run);
    printf("=========================================\n");
    
    return tests_passed == tests_run ? 0 : 1;
}
