#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define RTE_IPV4(a, b, c, d) ((uint32_t)(((a) << 24) | ((b) << 16) | ((c) << 8) | (d)))

struct route_entry {
    uint32_t next_hop_ip;
    uint16_t out_port;
};

struct route_bucket {
    uint32_t prefix;
    uint8_t depth;
    struct route_entry entry;
    uint8_t valid;
};

struct route_table {
    struct route_bucket *buckets;
    uint32_t capacity;
    uint32_t count;
};

int route_table_init(struct route_table *tbl, const char *name, uint32_t capacity, int socket_id) {
    (void)name; (void)socket_id;
    if (!tbl || capacity == 0) return -1;
    tbl->buckets = calloc(capacity, sizeof(struct route_bucket));
    if (!tbl->buckets) return -1;
    tbl->capacity = capacity;
    tbl->count = 0;
    return 0;
}

void route_table_free(struct route_table *tbl) {
    if (!tbl) return;
    if (tbl->buckets) free(tbl->buckets);
    tbl->buckets = NULL;
    tbl->capacity = 0;
}

int route_add(struct route_table *tbl, uint32_t prefix, uint8_t depth, const struct route_entry *entry) {
    if (!tbl || !tbl->buckets || !entry) return -1;
    if (depth > 32) return -1;
    
    for (uint32_t i = 0; i < tbl->capacity; i++) {
        if (!tbl->buckets[i].valid) {
            tbl->buckets[i].prefix = prefix;
            tbl->buckets[i].depth = depth;
            tbl->buckets[i].entry = *entry;
            tbl->buckets[i].valid = 1;
            tbl->count++;
            return 0;
        }
    }
    return -1;
}

static uint32_t mask_from_depth(uint8_t depth) {
    if (depth == 0) return 0;
    if (depth == 32) return 0xFFFFFFFF;
    return (((uint32_t)0xFFFFFFFF) << (32 - depth));
}

int route_lookup(const struct route_table *tbl, uint32_t ip, struct route_entry *result) {
    if (!tbl || !tbl->buckets || !result) return -1;
    
    struct route_bucket *best = NULL;
    uint8_t best_depth = 0;
    
    for (uint32_t i = 0; i < tbl->capacity; i++) {
        struct route_bucket *b = &tbl->buckets[i];
        if (!b->valid) continue;
        
        uint32_t mask = mask_from_depth(b->depth);
        if ((ip & mask) == (b->prefix & mask)) {
            if (b->depth >= best_depth) {
                best_depth = b->depth;
                best = b;
            }
        }
    }
    
    if (!best) return -1;
    *result = best->entry;
    return 0;
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

void test_route_init(void) {
    printf("\n=== test_route_init ===\n");
    struct route_table rt;
    
    ck_assert_int_eq(route_table_init(&rt, "test", 1024, 0), 0);
    ck_assert_ptr_ne(rt.buckets, NULL);
    ck_assert_int_eq(rt.capacity, 1024);
    
    route_table_free(&rt);
}

void test_route_add(void) {
    printf("\n=== test_route_add ===\n");
    struct route_table rt;
    
    route_table_init(&rt, "test", 1024, 0);
    
    struct route_entry entry = {
        .next_hop_ip = RTE_IPV4(192, 168, 0, 1),
        .out_port = 0,
    };
    
    ck_assert_int_eq(route_add(&rt, RTE_IPV4(0, 0, 0, 0), 0, &entry), 0);
    
    route_table_free(&rt);
}

void test_route_lookup_exact(void) {
    printf("\n=== test_route_lookup_exact ===\n");
    struct route_table rt;
    struct route_entry result;
    
    route_table_init(&rt, "test", 1024, 0);
    
    struct route_entry entry = {
        .next_hop_ip = RTE_IPV4(192, 168, 0, 1),
        .out_port = 0,
    };
    
    route_add(&rt, RTE_IPV4(0, 0, 0, 0), 0, &entry);
    
    ck_assert_int_eq(route_lookup(&rt, RTE_IPV4(8, 8, 8, 8), &result), 0);
    ck_assert_int_eq(result.next_hop_ip, RTE_IPV4(192, 168, 0, 1));
    ck_assert_int_eq(result.out_port, 0);
    
    route_table_free(&rt);
}

void test_route_lpm_longest_prefix(void) {
    printf("\n=== test_route_lpm_longest_prefix ===\n");
    struct route_table rt;
    struct route_entry result;
    
    route_table_init(&rt, "test", 1024, 0);
    
    struct route_entry default_route = {
        .next_hop_ip = RTE_IPV4(192, 168, 0, 1),
        .out_port = 0,
    };
    route_add(&rt, RTE_IPV4(0, 0, 0, 0), 0, &default_route);
    
    struct route_entry specific_route = {
        .next_hop_ip = RTE_IPV4(10, 0, 0, 1),
        .out_port = 1,
    };
    route_add(&rt, RTE_IPV4(10, 0, 0, 0), 24, &specific_route);
    
    ck_assert_int_eq(route_lookup(&rt, RTE_IPV4(10, 0, 0, 50), &result), 0);
    ck_assert_int_eq(result.next_hop_ip, RTE_IPV4(10, 0, 0, 1));
    ck_assert_int_eq(result.out_port, 1);
    
    ck_assert_int_eq(route_lookup(&rt, RTE_IPV4(192, 168, 1, 1), &result), 0);
    ck_assert_int_eq(result.next_hop_ip, RTE_IPV4(192, 168, 0, 1));
    ck_assert_int_eq(result.out_port, 0);
    
    route_table_free(&rt);
}

void test_route_no_match(void) {
    printf("\n=== test_route_no_match ===\n");
    struct route_table rt;
    struct route_entry result;
    
    route_table_init(&rt, "test", 1024, 0);
    
    struct route_entry entry = {
        .next_hop_ip = RTE_IPV4(192, 168, 0, 1),
        .out_port = 0,
    };
    route_add(&rt, RTE_IPV4(10, 0, 0, 0), 8, &entry);
    
    ck_assert_int_eq(route_lookup(&rt, RTE_IPV4(11, 0, 0, 1), &result), -1);
    
    route_table_free(&rt);
}

int main(void) {
    printf("=========================================\n");
    printf("Route Unit Tests (Standalone)\n");
    printf("=========================================\n");
    
    test_route_init();
    test_route_add();
    test_route_lookup_exact();
    test_route_lpm_longest_prefix();
    test_route_no_match();
    
    printf("\n=========================================\n");
    printf("Results: %d/%d tests passed\n", tests_passed, tests_run);
    printf("=========================================\n");
    
    return tests_passed == tests_run ? 0 : 1;
}
