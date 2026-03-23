#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>
#include <stdbool.h>

#define IPPROTO_TCP 6
#define IPPROTO_UDP 17

#define RTE_IPV4(a, b, c, d) ((uint32_t)(((a) << 24) | ((b) << 16) | ((c) << 8) | (d)))
#define RTE_CPU_TO_BE_32(x) ((((x) >> 24) & 0xFF) | (((x) >> 16) & 0xFF) << 8 | (((x) >> 8) & 0xFF) << 16 | (((x) & 0xFF) << 24))
#define RTE_BE_TO_CPU_32(x) RTE_CPU_TO_BE_32(x)

#pragma pack(push, 1)
typedef struct {
    uint32_t src_addr;
    uint32_t dst_addr;
    uint16_t src_port;
    uint16_t dst_port;
    uint8_t next_proto_id;
    uint8_t version_ihl;
    uint8_t time_to_live;
    uint16_t hdr_checksum;
} rte_ipv4_hdr;

typedef struct {
    uint16_t src_port;
    uint16_t dst_port;
    uint8_t tcp_flags;
} rte_tcp_hdr;
#pragma pack(pop)

typedef struct {} rte_rwlock_t;

void rte_rwlock_init(rte_rwlock_t *rwl) { (void)rwl; }
void rte_rwlock_read_lock(rte_rwlock_t *rwl) { (void)rwl; }
void rte_rwlock_read_unlock(rte_rwlock_t *rwl) { (void)rwl; }
void rte_rwlock_write_lock(rte_rwlock_t *rwl) { (void)rwl; }
void rte_rwlock_write_unlock(rte_rwlock_t *rwl) { (void)rwl; }

void *rte_zmalloc(const char *type, size_t size, unsigned align) {
    (void)type; (void)align;
    return calloc(1, size);
}
void *rte_malloc(const char *type, size_t size, unsigned align) {
    (void)type; (void)align;
    return malloc(size);
}
void rte_free(void *ptr) { free(ptr); }

#pragma pack(push, 1)
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
#pragma pack(pop)

int acl_init(struct acl_ctx *ctx, uint32_t capacity) {
    if (!ctx || capacity == 0) return -1;
    memset(ctx, 0, sizeof(*ctx));
    ctx->rules = rte_zmalloc("acl_rules", sizeof(struct acl_rule) * capacity, 0);
    if (!ctx->rules) return -1;
    ctx->capacity = capacity;
    rte_rwlock_init(&ctx->lock);
    return 0;
}

void acl_free(struct acl_ctx *ctx) {
    if (!ctx) return;
    if (ctx->rules) {
        rte_free(ctx->rules);
        ctx->rules = NULL;
    }
    ctx->count = 0;
    ctx->capacity = 0;
}

int acl_add_rule(struct acl_ctx *ctx, const struct acl_rule *rule) {
    if (!ctx || !ctx->rules || !rule) return -1;
    rte_rwlock_write_lock(&ctx->lock);
    if (ctx->count >= ctx->capacity) {
        rte_rwlock_write_unlock(&ctx->lock);
        return -1;
    }
    ctx->rules[ctx->count++] = *rule;
    rte_rwlock_write_unlock(&ctx->lock);
    return 0;
}

int acl_delete_rule(struct acl_ctx *ctx, uint32_t index) {
    if (!ctx || !ctx->rules) return -1;
    rte_rwlock_write_lock(&ctx->lock);
    if (index >= ctx->count) {
        rte_rwlock_write_unlock(&ctx->lock);
        return -1;
    }
    if (index + 1 < ctx->count) {
        memmove(&ctx->rules[index], &ctx->rules[index + 1],
            sizeof(struct acl_rule) * (ctx->count - index - 1));
    }
    ctx->count--;
    rte_rwlock_write_unlock(&ctx->lock);
    return 0;
}

void acl_clear(struct acl_ctx *ctx) {
    if (!ctx || !ctx->rules) return;
    rte_rwlock_write_lock(&ctx->lock);
    ctx->count = 0;
    rte_rwlock_write_unlock(&ctx->lock);
}

static bool acl_match_ports(const struct acl_rule *rule, uint8_t proto, uint16_t src_port, uint16_t dst_port) {
    if (!rule->match_ports) return true;
    if (rule->proto != 0 && rule->proto != proto) return false;
    if (src_port < rule->src_port_min || src_port > rule->src_port_max) return false;
    if (dst_port < rule->dst_port_min || dst_port > rule->dst_port_max) return false;
    return true;
}

bool acl_check_ipv4(const struct acl_ctx *ctx, const rte_ipv4_hdr *ip, const void *l4_hdr, uint32_t *deny_rule_index) {
    if (!ctx || !ctx->rules || !ip) {
        if (deny_rule_index) *deny_rule_index = 0xFFFFFFFF;
        return true;
    }
    uint32_t src = RTE_BE_TO_CPU_32(ip->src_addr);
    uint32_t dst = RTE_BE_TO_CPU_32(ip->dst_addr);
    uint8_t proto = ip->next_proto_id;
    uint16_t src_port = 0, dst_port = 0;

    if (l4_hdr && (proto == IPPROTO_TCP || proto == IPPROTO_UDP)) {
        if (proto == IPPROTO_TCP) {
            const uint8_t *p = (const uint8_t *)l4_hdr;
            src_port = (p[0] << 8) | p[1];
            dst_port = (p[2] << 8) | p[3];
        }
    }

    for (uint32_t i = 0; i < ctx->count; i++) {
        const struct acl_rule *rule = &ctx->rules[i];
        if (rule->src_mask && ((src & rule->src_mask) != (rule->src_ip & rule->src_mask))) continue;
        if (rule->dst_mask && ((dst & rule->dst_mask) != (rule->dst_ip & rule->dst_mask))) continue;
        if (!acl_match_ports(rule, proto, src_port, dst_port)) continue;
        if (!rule->allow) {
            if (deny_rule_index) *deny_rule_index = i;
            return false;
        }
    }
    if (deny_rule_index) *deny_rule_index = 0xFFFFFFFF;
    return true;
}

static int tests_run = 0;
static int tests_passed = 0;

#define ck_assert_int_eq(a, b) do { \
    tests_run++; \
    int _a = (a); int _b = (b); \
    if (_a == _b) { \
        tests_passed++; \
        printf("  PASS: %s == %s (%d == %d)\n", #a, #b, _a, _b); \
    } else { \
        printf("  FAIL: %s != %s (%d != %d)\n", #a, #b, _a, _b); \
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

void test_acl_init(void) {
    printf("\n=== test_acl_init ===\n");
    struct acl_ctx ctx;
    
    ck_assert_int_eq(acl_init(&ctx, 16), 0);
    ck_assert_ptr_ne(ctx.rules, NULL);
    ck_assert_int_eq(ctx.count, 0);
    ck_assert_int_eq(ctx.capacity, 16);
    acl_free(&ctx);
    
    ck_assert_int_eq(acl_init(NULL, 16), -1);
    ck_assert_int_eq(acl_init(&ctx, 0), -1);
}

void test_acl_add_rule(void) {
    printf("\n=== test_acl_add_rule ===\n");
    struct acl_ctx ctx;
    struct acl_rule rule = {
        .src_ip = RTE_IPV4(192, 168, 1, 0),
        .src_mask = RTE_IPV4(255, 255, 255, 0),
        .dst_ip = 0,
        .dst_mask = 0,
        .allow = 1,
    };
    
    ck_assert_int_eq(acl_init(&ctx, 16), 0);
    ck_assert_int_eq(acl_add_rule(&ctx, &rule), 0);
    ck_assert_int_eq(ctx.count, 1);
    ck_assert_int_eq(ctx.rules[0].src_ip, rule.src_ip);
    ck_assert_int_eq(ctx.rules[0].allow, 1);
    ck_assert_int_eq(acl_add_rule(NULL, &rule), -1);
    acl_free(&ctx);
}

void test_acl_delete_rule(void) {
    printf("\n=== test_acl_delete_rule ===\n");
    struct acl_ctx ctx;
    struct acl_rule rule = {
        .src_ip = RTE_IPV4(10, 0, 0, 0),
        .src_mask = RTE_IPV4(255, 0, 0, 0),
        .allow = 1,
    };
    
    ck_assert_int_eq(acl_init(&ctx, 16), 0);
    ck_assert_int_eq(acl_add_rule(&ctx, &rule), 0);
    ck_assert_int_eq(ctx.count, 1);
    ck_assert_int_eq(acl_delete_rule(&ctx, 0), 0);
    ck_assert_int_eq(ctx.count, 0);
    ck_assert_int_eq(acl_delete_rule(&ctx, 0), -1);
    ck_assert_int_eq(acl_delete_rule(NULL, 0), -1);
    acl_free(&ctx);
}

void test_acl_ipv4_match(void) {
    printf("\n=== test_acl_ipv4_match ===\n");
    struct acl_ctx ctx;
    struct acl_rule deny_rule = {
        .src_ip = RTE_IPV4(10, 0, 0, 0),
        .src_mask = RTE_IPV4(255, 0, 0, 0),
        .allow = 0,
    };
    
    ck_assert_int_eq(acl_init(&ctx, 16), 0);
    ck_assert_int_eq(acl_add_rule(&ctx, &deny_rule), 0);
    
    rte_ipv4_hdr ip = {
        .src_addr = RTE_CPU_TO_BE_32(RTE_IPV4(10, 1, 1, 100)),
        .dst_addr = RTE_CPU_TO_BE_32(RTE_IPV4(192, 168, 1, 1)),
        .next_proto_id = IPPROTO_TCP,
    };
    
    uint32_t deny_idx;
    ck_assert(!acl_check_ipv4(&ctx, &ip, NULL, &deny_idx));
    ck_assert_int_eq(deny_idx, 0);
    
    rte_ipv4_hdr ip_allowed = {
        .src_addr = RTE_CPU_TO_BE_32(RTE_IPV4(192, 168, 1, 100)),
        .dst_addr = RTE_CPU_TO_BE_32(RTE_IPV4(10, 0, 0, 1)),
        .next_proto_id = IPPROTO_TCP,
    };
    ck_assert(acl_check_ipv4(&ctx, &ip_allowed, NULL, NULL));
    acl_free(&ctx);
}

void test_acl_port_range_match(void) {
    printf("\n=== test_acl_port_range_match ===\n");
    struct acl_ctx ctx;
    struct acl_rule http_rule = {
        .proto = IPPROTO_TCP,
        .dst_port_min = 80,
        .dst_port_max = 80,
        .match_ports = 1,
        .allow = 1,
    };
    
    ck_assert_int_eq(acl_init(&ctx, 16), 0);
    ck_assert_int_eq(acl_add_rule(&ctx, &http_rule), 0);
    
    rte_ipv4_hdr ip = { .next_proto_id = IPPROTO_TCP };
    uint8_t tcp_80[4] = { 0, 0, 0, 80 };
    uint8_t tcp_443[4] = { 0, 0, 1, 0xBB };
    
    ck_assert(acl_check_ipv4(&ctx, &ip, tcp_80, NULL));
    
    ck_assert(!acl_check_ipv4(&ctx, &ip, tcp_443, NULL));
    
    acl_free(&ctx);
}

void test_acl_clear(void) {
    printf("\n=== test_acl_clear ===\n");
    struct acl_ctx ctx;
    struct acl_rule rule = { .allow = 1 };
    
    ck_assert_int_eq(acl_init(&ctx, 16), 0);
    for (int i = 0; i < 5; i++) ck_assert_int_eq(acl_add_rule(&ctx, &rule), 0);
    ck_assert_int_eq(ctx.count, 5);
    acl_clear(&ctx);
    ck_assert_int_eq(ctx.count, 0);
    acl_free(&ctx);
}

int main(void) {
    printf("=========================================\n");
    printf("ACL Unit Tests (Standalone)\n");
    printf("=========================================\n");
    
    test_acl_init();
    test_acl_add_rule();
    test_acl_delete_rule();
    test_acl_ipv4_match();
    test_acl_port_range_match();
    test_acl_clear();
    
    printf("\n=========================================\n");
    printf("Results: %d/%d tests passed\n", tests_passed, tests_run);
    printf("=========================================\n");
    
    return tests_passed == tests_run ? 0 : 1;
}
