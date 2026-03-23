# DPDK Packet Firewall Testing Guide

## 1. Testing Overview

### 1.1 Testing Objectives
- Ensure correctness and stability of packet processing pipeline
- Verify ACL rule engine matching logic
- Ensure session tracking and routing functions work properly
- Verify IPC communication between control plane and data plane
- Guarantee reliability of frontend Vue.js components

### 1.2 Testing Scope
| Module | Priority | Description |
|--------|----------|-------------|
| ACL (IPv4/IPv6) | High | Rule add/delete/match |
| Session (IPv4/IPv6) | High | Session create/find/expire |
| Route (IPv4/IPv6) | High | LPM route lookup |
| ARP/ND | Medium | MAC/ND address resolution |
| Rate Limiting | Medium | Token Bucket algorithm |
| IPC | High | Ring + Shared Memory |
| Frontend | Medium | API + Vue components |

### 1.3 Testing Environment Requirements
```bash
# System Requirements
- Ubuntu 22.04 LTS (64-bit)
- Linux kernel 5.15.0+
- DPDK 24.11
- GCC 11+ / Clang
- Node.js 18+

# Frontend Testing Dependencies
- vitest (integrated with Vite)
- @vue/test-utils
- @testing-library/vue
```

---

## 2. Unit Testing Specification

### 2.1 Test File Structure
```
dataplane/
├── test/
│   ├── test_acl.c          # ACL unit tests
│   ├── test_acl6.c        # IPv6 ACL unit tests
│   ├── test_session.c     # Session unit tests
│   ├── test_session6.c    # IPv6 Session unit tests
│   ├── test_route.c       # Route unit tests
│   ├── test_route6.c      # IPv6 Route unit tests
│   ├── test_arp.c         # ARP unit tests
│   ├── test_nd.c          # ND unit tests
│   ├── mock_dpdk.h        # DPDK mock header
│   ├── mock_dpdk.c        # DPDK mock implementation
│   └── meson.build        # Test build configuration
```

### 2.2 DPDK Mock Strategy

Due to many DPDK dependencies, a mock layer is required:

#### 2.2.1 Mock Header (`dataplane/test/mock_dpdk.h`)
```c
#pragma once

#include <stdint.h>
#include <string.h>
#include <stdbool.h>

#define RTE_MAX_LCORE 64
#define RTE_MAX_ETHPORTS 16
#define RTE_ETHER_ADDR_LEN 6

typedef struct {
    uint8_t addr_bytes[RTE_ETHER_ADDR_LEN];
} __attribute__((aligned(1))) rte_ether_addr;

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
    uint16_t src_port;
    uint16_t dst_port;
    uint8_t tcp_flags;
} __attribute__((aligned(1))) rte_tcp_hdr;

typedef struct {
    uint16_t src_port;
    uint16_t dst_port;
} __attribute__((aligned(1))) rte_udp_hdr;

#define IPPROTO_TCP 6
#define IPPROTO_UDP 17

#define RTE_IPV4(a, b, c, d) ((uint32_t)(((a) << 24) | ((b) << 16) | ((c) << 8) | (d)))
#define RTE_IPV4_MASK(a, b, c, d) RTE_IPV4(a, b, c, d)

uint16_t rte_be_to_cpu_16(uint16_t x);
uint16_t rte_cpu_to_be_16(uint16_t x);
uint32_t rte_be_to_cpu_32(uint32_t x);
```

#### 2.2.2 Mock Implementation (`dataplane/test/mock_dpdk.c`)
```c
#include "mock_dpdk.h"

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
```

### 2.3 ACL Module Test Cases

#### 2.3.1 test_acl_init
```c
void test_acl_init(void) {
    struct acl_ctx ctx;
    
    // Valid initialization
    ck_assert_int_eq(acl_init(&ctx, 16), 0);
    ck_assert_ptr_ne(ctx.rules, NULL);
    ck_assert_int_eq(ctx.count, 0);
    ck_assert_int_eq(ctx.capacity, 16);
    acl_free(&ctx);
    
    // Invalid initialization (null context)
    ck_assert_int_eq(acl_init(NULL, 16), -1);
    
    // Invalid initialization (zero capacity)
    ck_assert_int_eq(acl_init(&ctx, 0), -1);
}
```

#### 2.3.2 test_acl_add_rule
```c
void test_acl_add_rule(void) {
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
    
    acl_free(&ctx);
}
```

#### 2.3.3 test_acl_ipv4_match
```c
void test_acl_ipv4_match(void) {
    struct acl_ctx ctx;
    struct acl_rule deny_rule = {
        .src_ip = RTE_IPV4(10, 0, 0, 0),
        .src_mask = RTE_IPV4(255, 0, 0, 0),
        .allow = 0,
    };
    
    acl_init(&ctx, 16);
    acl_add_rule(&ctx, &deny_rule);
    
    struct rte_ipv4_hdr ip = {
        .src_addr = RTE_CPU_TO_BE_32(RTE_IPV4(10, 1, 1, 100)),
        .dst_addr = RTE_CPU_TO_BE_32(RTE_IPV4(192, 168, 1, 1)),
        .next_proto_id = IPPROTO_TCP,
    };
    
    uint32_t deny_idx;
    ck_assert(!acl_check_ipv4(&ctx, &ip, NULL, &deny_idx));
    ck_assert_int_eq(deny_idx, 0);
    
    acl_free(&ctx);
}
```

#### 2.3.4 test_acl_port_range_match
```c
void test_acl_port_range_match(void) {
    struct acl_ctx ctx;
    struct acl_rule http_rule = {
        .proto = IPPROTO_TCP,
        .dst_port_min = 80,
        .dst_port_max = 80,
        .match_ports = 1,
        .allow = 1,
    };
    
    acl_init(&ctx, 16);
    acl_add_rule(&ctx, &http_rule);
    
    struct rte_ipv4_hdr ip = {
        .next_proto_id = IPPROTO_TCP,
    };
    
    struct rte_tcp_hdr tcp_80 = {
        .dst_port = RTE_CPU_TO_BE_16(80),
    };
    ck_assert(acl_check_ipv4(&ctx, &ip, &tcp_80, NULL));
    
    struct rte_tcp_hdr tcp_443 = {
        .dst_port = RTE_CPU_TO_BE_16(443),
    };
    ck_assert(!acl_check_ipv4(&ctx, &ip, &tcp_443, NULL));
    
    acl_free(&ctx);
}
```

### 2.4 Session Module Test Cases

#### 2.4.1 test_session_create
```c
void test_session_create(void) {
    struct session_table tbl;
    
    session_table_init(&tbl, "test", 1024, 0);
    ck_assert_int_eq(tbl.count, 0);
    
    struct session_key key = {
        .src_ip = RTE_IPV4(192, 168, 1, 100),
        .dst_ip = RTE_IPV4(10, 0, 0, 1),
        .src_port = 12345,
        .dst_port = 80,
        .proto = IPPROTO_TCP,
    };
    
    session_track(&tbl, &key, 64, 1000, NULL);
    ck_assert_int_eq(tbl.count, 1);
    
    session_table_free(&tbl);
}
```

#### 2.4.2 test_session_lookup
```c
void test_session_lookup(void) {
    struct session_table tbl;
    struct session_entry entry;
    
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
    ck_assert_int_eq(found->packets, 1);
    ck_assert_int_eq(found->bytes, 64);
    
    session_table_free(&tbl);
}
```

#### 2.4.3 test_session_expiration
```c
void test_session_expiration(void) {
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
```

### 2.5 Route Module Test Cases

#### 2.5.1 test_route_lpm_add
```c
void test_route_lpm_add(void) {
    struct route_table rt;
    
    route_table_init(&rt, "test", 1024, 0);
    
    struct route_entry entry = {
        .next_hop_ip = RTE_IPV4(192, 168, 0, 1),
        .out_port = 0,
    };
    
    ck_assert_int_eq(route_add(&rt, RTE_IPV4(0, 0, 0, 0), 0, &entry), 0);
    
    struct route_entry result;
    ck_assert_int_eq(route_lookup(&rt, RTE_IPV4(8, 8, 8, 8), &result), 0);
    ck_assert_int_eq(result.next_hop_ip, RTE_IPV4(192, 168, 0, 1));
    
    route_table_free(&rt);
}
```

#### 2.5.2 test_route_lpm_longest_prefix
```c
void test_route_lpm_longest_prefix(void) {
    struct route_table rt;
    
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
    
    struct route_entry result;
    
    ck_assert_int_eq(route_lookup(&rt, RTE_IPV4(192, 168, 1, 1), &result), 0);
    ck_assert_int_eq(result.next_hop_ip, RTE_IPV4(192, 168, 0, 1));
    
    ck_assert_int_eq(route_lookup(&rt, RTE_IPV4(10, 0, 0, 50), &result), 0);
    ck_assert_int_eq(result.next_hop_ip, RTE_IPV4(10, 0, 0, 1));
    
    route_table_free(&rt);
}
```

---

## 3. Meson Test Build Configuration

### 3.1 dataplane/test/meson.build
```meson
project('dpdk_packet_firewall_tests', 'c',
    default_options: ['c_std=gnu11', 'warning_level=2'])

dpdk_dep = dependency('libdpdk', required: true)

# Mock DPDK implementation
mock_dpdk_lib = static_library(
  'mock_dpdk',
  'mock_dpdk.c',
  dependencies: dpdk_dep,
)

# ACL Tests
test_acl_sources = [
  'test_acl.c',
  '../acl/acl.c',
  'mock_dpdk.c',
]

test_acl_exec = executable(
  'test_acl',
  test_acl_sources,
  dependencies: dpdk_dep,
)
test('acl_basic', test_acl_exec, suite: 'dataplane')

# Session Tests
test_session_sources = [
  'test_session.c',
  '../session/session.c',
  'mock_dpdk.c',
]

test_session_exec = executable(
  'test_session',
  test_session_sources,
  dependencies: dpdk_dep,
)
test('session_basic', test_session_exec, suite: 'dataplane')

# Route Tests
test_route_sources = [
  'test_route.c',
  '../route/route.c',
  'mock_dpdk.c',
]

test_route_exec = executable(
  'test_route',
  test_route_sources,
  dependencies: dpdk_dep,
)
test('route_basic', test_route_exec, suite: 'dataplane')
```

---

## 4. Frontend Testing Specification (Vitest)

### 4.1 Test File Structure
```
web/frontend/src/
├── components/
│   ├── __tests__/
│   │   ├── RuleTable.test.ts
│   │   └── RuleForm.test.ts
│   └── RuleTable.vue
│
├── pages/
│   ├── __tests__/
│   │   ├── AclPage.test.ts
│   │   └── DashboardPage.test.ts
│   └── AclPage.vue
│
├── services/
│   └── __tests__/
│       └── api.test.ts
```

### 4.2 Vitest Configuration
```typescript
// web/frontend/vite.config.ts
import { defineConfig } from 'vite'
import vue from '@vitejs/plugin-vue'
import { resolve } from 'path'

export default defineConfig({
  plugins: [vue()],
  server: {
    host: true,
    port: 5173,
    proxy: {
      '/api': {
        target: 'http://127.0.0.1:9000',
        changeOrigin: true
      }
    }
  },
  test: {
    globals: true,
    environment: 'jsdom',
    include: ['src/**/*.{test,spec}.{js,ts}'],
    coverage: {
      provider: 'v8',
      reporter: ['text', 'json', 'html'],
      thresholds: {
        lines: 80,
        functions: 80,
        branches: 70,
      },
    },
  },
  resolve: {
    alias: {
      '@': resolve(__dirname, 'src'),
    },
  },
})
```

### 4.3 API Service Tests
```typescript
// web/frontend/src/services/__tests__/api.test.ts
import { describe, it, expect, vi, beforeEach } from 'vitest'
import axios from 'axios'
import { getAclSnapshot, addAclRule, deleteAclRule, clearAcl } from '../api'

vi.mock('axios')

const mockedAxios = vi.mocked(axios.create)

describe('API Service', () => {
  beforeEach(() => {
    vi.clearAllMocks()
  })

  describe('getAclSnapshot', () => {
    it('should return ACL snapshot with rules array', async () => {
      const mockData = {
        version: 1,
        count: 2,
        rules: [
          { index: 0, action: 'deny', src: '10.0.0.0/8', dst: 'any', proto: 'any', sport: 'any', dport: 'any' },
          { index: 1, action: 'allow', src: 'any', dst: 'any', proto: 'any', sport: 'any', dport: 'any' },
        ],
      }
      mockedAxios.mockResolvedValue({ data: mockData })

      const result = await getAclSnapshot()
      
      expect(result.version).toBe(1)
      expect(result.rules).toHaveLength(2)
      expect(result.rules[0].action).toBe('deny')
    })

    it('should handle API errors gracefully', async () => {
      mockedAxios.mockRejectedValue(new Error('Network error'))

      await expect(getAclSnapshot()).rejects.toThrow('Network error')
    })
  })

  describe('addAclRule', () => {
    it('should send correct payload to API', async () => {
      const payload = {
        action: 'allow',
        src: '192.168.1.0/24',
        dst: 'any',
        proto: 6,
        src_port_min: 0,
        src_port_max: 0,
        dst_port_min: 80,
        dst_port_max: 80,
      }
      
      mockedAxios.mockResolvedValue({ data: null })
      await addAclRule(payload)
      
      expect(mockedAxios).toHaveBeenCalledWith('/api/acl', payload, undefined)
    })
  })

  describe('deleteAclRule', () => {
    it('should send DELETE request with correct index', async () => {
      mockedAxios.mockResolvedValue({ data: null })
      await deleteAclRule(5)
      
      expect(mockedAxios).toHaveBeenCalledWith('/api/acl/5', undefined, expect.any(Object))
    })
  })

  describe('clearAcl', () => {
    it('should send DELETE request to clear all rules', async () => {
      mockedAxios.mockResolvedValue({ data: null })
      await clearAcl()
      
      expect(mockedAxios).toHaveBeenCalledWith('/api/acl/', undefined, expect.any(Object))
    })
  })
})
```

### 4.4 Vue Component Tests
```typescript
// web/frontend/src/components/__tests__/RuleTable.test.ts
import { describe, it, expect } from 'vitest'
import { mount } from '@vue/test-utils'
import { defineComponent, h } from 'vue'
import RuleTable from '../RuleTable.vue'

const MockRuleTable = defineComponent({
  props: {
    rows: { type: Array, default: () => [] },
    loading: { type: Boolean, default: false },
  },
  emits: ['delete'],
  setup(props, { emit }) {
    return () => h('div', { class: 'mock-rule-table' }, [
      h('table', {}, 
        props.rows.map((row: any, idx: number) =>
          h('tr', { key: idx }, [
            h('td', {}, row.action),
            h('td', {}, row.src),
            h('td', {}, [
              h('button', { 
                class: 'delete-btn',
                onClick: () => emit('delete', row.index)
              }, 'Delete')
            ])
          ])
        )
      ),
      props.loading ? h('div', { class: 'el-table__loading' }, 'Loading...') : null
    ])
  }
})

describe('RuleTable', () => {
  const mockRows = [
    { index: 0, action: 'deny', src: '10.0.0.0/8', dst: 'any', deny_pkts: 100, deny_bytes: 6400 },
    { index: 1, action: 'allow', src: 'any', dst: 'any', deny_pkts: 0, deny_bytes: 0 },
  ]

  it('should render table with rows', () => {
    const wrapper = mount(MockRuleTable as any, {
      props: { rows: mockRows, loading: false },
    })
    
    expect(wrapper.findAll('tr')).toHaveLength(mockRows.length)
  })

  it('should emit delete event with correct index', async () => {
    const wrapper = mount(MockRuleTable as any, {
      props: { rows: mockRows, loading: false },
    })
    
    await wrapper.find('.delete-btn').trigger('click')
    
    expect(wrapper.emitted('delete')).toBeTruthy()
    expect(wrapper.emitted('delete')[0]).toEqual([0])
  })

  it('should show loading state', () => {
    const wrapper = mount(MockRuleTable as any, {
      props: { rows: [], loading: true },
    })
    
    expect(wrapper.find('.el-table__loading').exists()).toBe(true)
  })
})
```

### 4.5 Page Integration Tests
```typescript
// web/frontend/src/pages/__tests__/AclPage.test.ts
import { describe, it, expect, vi, beforeEach } from 'vitest'
import { mount, flushPromises } from '@vue/test-utils'
import { createRouter, createWebHistory } from 'vue-router'
import AclPage from '../AclPage.vue'
import * as api from '@/services/api'

vi.mock('@/services/api')

describe('AclPage', () => {
  const router = createRouter({
    history: createWebHistory(),
    routes: [{ path: '/acl', component: AclPage }],
  })

  beforeEach(() => {
    vi.clearAllMocks()
  })

  it('should load ACL rules on mount', async () => {
    const mockSnapshot = {
      version: 1,
      count: 1,
      rules: [{ index: 0, action: 'allow', src: 'any', dst: 'any', proto: 'any', sport: 'any', dport: 'any' }],
    }
    vi.mocked(api.getAclSnapshot).mockResolvedValue(mockSnapshot)
    vi.mocked(api.getAclHits).mockResolvedValue({ version: 1, rule_version: 1, count: 1, pkts: [0], bytes: [0] })

    const wrapper = mount(AclPage, {
      global: { plugins: [router] },
    })
    await flushPromises()

    expect(api.getAclSnapshot).toHaveBeenCalled()
  })

  it('should display rule count', async () => {
    const mockSnapshot = {
      version: 1,
      count: 2,
      rules: [
        { index: 0, action: 'deny', src: '10.0.0.0/8', dst: 'any', proto: 'any', sport: 'any', dport: 'any' },
        { index: 1, action: 'allow', src: 'any', dst: 'any', proto: 'any', sport: 'any', dport: 'any' },
      ],
    }
    vi.mocked(api.getAclSnapshot).mockResolvedValue(mockSnapshot)
    vi.mocked(api.getAclHits).mockResolvedValue({ version: 1, rule_version: 1, count: 2, pkts: [10, 0], bytes: [1000, 0] })

    const wrapper = mount(AclPage, {
      global: { plugins: [router] },
    })
    await flushPromises()

    expect(wrapper.text()).toContain('总数 2')
  })
})
```

---

## 5. Test Coverage Requirements

### 5.1 Coverage Targets
| Type | Target | Description |
|------|--------|-------------|
| Line Coverage | ≥80% | All executable code |
| Function Coverage | ≥80% | All non-static functions |
| Branch Coverage | ≥70% | Conditional branches |
| Critical Paths | 100% | ACL match, Session lookup, IPC |

### 5.2 Running Coverage
```bash
# C Unit Tests (using gcov/lcov)
gcc -fprofile-arcs -ftest-coverage test_acl.c acl.c -o test_acl
./test_acl
gcov test_acl.c
lcov --capture --directory . --output coverage.info

# Frontend Coverage
npm run test -- --coverage
```

---

## 6. Running Tests

### 6.1 Build and Run Tests
```bash
# Full build with tests
meson setup build
ninja -C build
ninja -C build test

# Run specific module tests
./build/dataplane/test_acl
./build/dataplane/test_session
./build/dataplane/test_route

# Frontend tests with coverage
cd web/frontend
npm install
npm run test -- --coverage
```

### 6.2 Test Development Workflow
```bash
# Modify code then quickly rebuild single test
ninja -C build dataplane/test_acl.p/test_acl.c.o
ninja -C build test_acl
./build/dataplane/test_acl

# Frontend hot-reload testing
npm run test -- --watch
```

---

## Appendix A: Test Data Structures

### ACL Rule Test Cases
```c
struct acl_rule deny_private_10 = {
    .src_ip = RTE_IPV4(10, 0, 0, 0),
    .src_mask = RTE_IPV4(255, 0, 0, 0),
    .allow = 0,
};

struct acl_rule deny_private_172 = {
    .src_ip = RTE_IPV4(172, 16, 0, 0),
    .src_mask = RTE_IPV4(255, 240, 0, 0),
    .allow = 0,
};

struct acl_rule deny_private_192 = {
    .src_ip = RTE_IPV4(192, 168, 0, 0),
    .src_mask = RTE_IPV4(255, 255, 0, 0),
    .allow = 0,
};

struct acl_rule allow_http = {
    .proto = IPPROTO_TCP,
    .dst_port_min = 80,
    .dst_port_max = 80,
    .match_ports = 1,
    .allow = 1,
};

struct acl_rule allow_https = {
    .proto = IPPROTO_TCP,
    .dst_port_min = 443,
    .dst_port_max = 443,
    .match_ports = 1,
    .allow = 1,
};

struct acl_rule allow_any = {
    .src_ip = 0,
    .src_mask = 0,
    .dst_ip = 0,
    .dst_mask = 0,
    .proto = 0,
    .match_ports = 0,
    .allow = 1,
};
```

### Session Key Test Cases
```c
struct session_key tcp_web = {
    .src_ip = RTE_IPV4(192, 168, 1, 100),
    .dst_ip = RTE_IPV4(10, 0, 0, 1),
    .src_port = 12345,
    .dst_port = 80,
    .proto = IPPROTO_TCP,
};

struct session_key tcp_https = {
    .src_ip = RTE_IPV4(192, 168, 1, 100),
    .dst_ip = RTE_IPV4(10, 0, 0, 1),
    .src_port = 54321,
    .dst_port = 443,
    .proto = IPPROTO_TCP,
};

struct session_key udp_dns = {
    .src_ip = RTE_IPV4(192, 168, 1, 100),
    .dst_ip = RTE_IPV4(8, 8, 8, 8),
    .src_port = 54321,
    .dst_port = 53,
    .proto = IPPROTO_UDP,
};
```

---

## Appendix B: Recommended Testing Tools

| Tool | Purpose |
|------|---------|
| Check Framework | C unit testing framework |
| cmocka | C mocking framework |
| gcov/lcov | Coverage tools |
| Vitest | Vue.js testing framework |
| @vue/test-utils | Vue component testing |
| MSW | API mock tool (Mock Service Worker) |
