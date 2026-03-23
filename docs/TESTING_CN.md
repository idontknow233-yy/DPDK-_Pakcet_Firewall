# DPDK Packet Firewall 测试文档

## 1. 测试概述

### 1.1 测试目标
- 确保数据包处理流水线的正确性和稳定性
- 验证 ACL 规则引擎的匹配逻辑
- 确保会话跟踪和路由功能正常
- 保证控制平面与数据平面间的 IPC 通信
- 保证前端 Vue.js 组件的可靠性

### 1.2 测试范围
| 模块 | 优先级 | 说明 |
|------|--------|------|
| ACL (IPv4/IPv6) | 高 | 规则添加/删除/匹配 |
| Session (IPv4/IPv6) | 高 | 会话创建/查找/过期 |
| Route (IPv4/IPv6) | 高 | LPM 路由查找 |
| ARP/ND | 中 | MAC/ND 地址解析 |
| Rate Limiting | 中 | Token Bucket 算法 |
| IPC | 高 | Ring + Shared Memory |
| Frontend | 中 | API + Vue 组件 |

### 1.3 测试环境要求
```bash
# 系统要求
- Ubuntu 22.04 LTS (64-bit)
- Linux kernel 5.15.0+
- DPDK 24.11
- GCC 11+ / Clang
- Node.js 18+

# 前端测试依赖
- vitest (集成于 Vite)
- @vue/test-utils
- @testing-library/vue
```

---

## 2. 单元测试规范

### 2.1 测试文件结构
```
dataplane/
├── test/
│   ├── test_acl.c          # ACL 单元测试
│   ├── test_acl6.c         # IPv6 ACL 单元测试
│   ├── test_session.c      # Session 单元测试
│   ├── test_session6.c     # IPv6 Session 单元测试
│   ├── test_route.c        # Route 单元测试
│   ├── test_route6.c       # IPv6 Route 单元测试
│   ├── test_arp.c          # ARP 单元测试
│   ├── test_nd.c           # ND 单元测试
│   ├── mock_dpdk.h         # DPDK Mock 头文件
│   ├── mock_dpdk.c         # DPDK Mock 实现
│   └── meson.build         # 测试构建配置
```

### 2.2 DPDK Mock 策略

由于 DPDK 依赖较多，需要实现 Mock 层：

#### 2.2.1 Mock 头文件 (`dataplane/test/mock_dpdk.h`)
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

#### 2.2.2 Mock 实现 (`dataplane/test/mock_dpdk.c`)
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

### 2.3 ACL 模块测试用例

#### 2.3.1 test_acl_init 初始化测试
```c
void test_acl_init(void) {
    struct acl_ctx ctx;
    
    // 正常初始化
    ck_assert_int_eq(acl_init(&ctx, 16), 0);
    ck_assert_ptr_ne(ctx.rules, NULL);
    ck_assert_int_eq(ctx.count, 0);
    ck_assert_int_eq(ctx.capacity, 16);
    acl_free(&ctx);
    
    // 无效初始化（空指针）
    ck_assert_int_eq(acl_init(NULL, 16), -1);
    
    // 无效初始化（容量为0）
    ck_assert_int_eq(acl_init(&ctx, 0), -1);
}
```

#### 2.3.2 test_acl_add_rule 规则添加测试
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

#### 2.3.3 test_acl_ipv4_match IPv4 匹配测试
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

#### 2.3.4 test_acl_port_range_match 端口范围匹配测试
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
    
    // 端口80应匹配
    struct rte_tcp_hdr tcp_80 = {
        .dst_port = RTE_CPU_TO_BE_16(80),
    };
    ck_assert(acl_check_ipv4(&ctx, &ip, &tcp_80, NULL));
    
    // 端口443不应匹配
    struct rte_tcp_hdr tcp_443 = {
        .dst_port = RTE_CPU_TO_BE_16(443),
    };
    ck_assert(!acl_check_ipv4(&ctx, &ip, &tcp_443, NULL));
    
    acl_free(&ctx);
}
```

### 2.4 Session 模块测试用例

#### 2.4.1 test_session_create 会话创建测试
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

#### 2.4.2 test_session_lookup 会话查找测试
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

#### 2.4.3 test_session_expiration 会话过期测试
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
    
    // 超时过期
    session_soft_expire(&tbl, 10000, 100);
    ck_assert_int_eq(tbl.count, 0);
    
    session_table_free(&tbl);
}
```

### 2.5 Route 模块测试用例

#### 2.5.1 test_route_lpm_add 路由添加测试
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

#### 2.5.2 test_route_lpm_longest_prefix 最长前缀匹配测试
```c
void test_route_lpm_longest_prefix(void) {
    struct route_table rt;
    
    route_table_init(&rt, "test", 1024, 0);
    
    // 默认路由
    struct route_entry default_route = {
        .next_hop_ip = RTE_IPV4(192, 168, 0, 1),
        .out_port = 0,
    };
    route_add(&rt, RTE_IPV4(0, 0, 0, 0), 0, &default_route);
    
    // 特定路由
    struct route_entry specific_route = {
        .next_hop_ip = RTE_IPV4(10, 0, 0, 1),
        .out_port = 1,
    };
    route_add(&rt, RTE_IPV4(10, 0, 0, 0), 24, &specific_route);
    
    struct route_entry result;
    
    // 10.0.0.50 匹配 /24 特定路由
    ck_assert_int_eq(route_lookup(&rt, RTE_IPV4(10, 0, 0, 50), &result), 0);
    ck_assert_int_eq(result.next_hop_ip, RTE_IPV4(10, 0, 0, 1));
    
    // 192.168.1.1 匹配默认路由
    ck_assert_int_eq(route_lookup(&rt, RTE_IPV4(192, 168, 1, 1), &result), 0);
    ck_assert_int_eq(result.next_hop_ip, RTE_IPV4(192, 168, 0, 1));
    
    route_table_free(&rt);
}
```

---

## 3. Meson 测试构建配置

### 3.1 dataplane/test/meson.build
```meson
project('dpdk_packet_firewall_tests', 'c',
    default_options: ['c_std=gnu11', 'warning_level=2'])

dpdk_dep = dependency('libdpdk', required: true)

# Mock DPDK 实现
mock_dpdk_lib = static_library(
  'mock_dpdk',
  'mock_dpdk.c',
  dependencies: dpdk_dep,
)

# ACL 测试
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

# Session 测试
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

# Route 测试
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

## 4. 前端测试规范 (Vitest)

### 4.1 测试文件结构
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

### 4.2 Vitest 配置
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

### 4.3 API Service 测试
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
    it('应返回包含规则数组的 ACL 快照', async () => {
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

    it('应优雅处理 API 错误', async () => {
      mockedAxios.mockRejectedValue(new Error('Network error'))

      await expect(getAclSnapshot()).rejects.toThrow('Network error')
    })
  })

  describe('addAclRule', () => {
    it('应发送正确的载荷到 API', async () => {
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
    it('应发送带有正确索引的 DELETE 请求', async () => {
      mockedAxios.mockResolvedValue({ data: null })
      await deleteAclRule(5)
      
      expect(mockedAxios).toHaveBeenCalledWith('/api/acl/5', undefined, expect.any(Object))
    })
  })

  describe('clearAcl', () => {
    it('应发送 DELETE 请求以清空所有规则', async () => {
      mockedAxios.mockResolvedValue({ data: null })
      await clearAcl()
      
      expect(mockedAxios).toHaveBeenCalledWith('/api/acl/', undefined, expect.any(Object))
    })
  })
})
```

### 4.4 Vue 组件测试
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

  it('应渲染包含行的表格', () => {
    const wrapper = mount(MockRuleTable as any, {
      props: { rows: mockRows, loading: false },
    })
    
    expect(wrapper.findAll('tr')).toHaveLength(mockRows.length)
  })

  it('应发射带有正确索引的删除事件', async () => {
    const wrapper = mount(MockRuleTable as any, {
      props: { rows: mockRows, loading: false },
    })
    
    await wrapper.find('.delete-btn').trigger('click')
    
    expect(wrapper.emitted('delete')).toBeTruthy()
    expect(wrapper.emitted('delete')[0]).toEqual([0])
  })

  it('应显示加载状态', () => {
    const wrapper = mount(MockRuleTable as any, {
      props: { rows: [], loading: true },
    })
    
    expect(wrapper.find('.el-table__loading').exists()).toBe(true)
  })
})
```

### 4.5 页面集成测试
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

  it('应在挂载时加载 ACL 规则', async () => {
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

  it('应显示规则数量', async () => {
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

## 5. 测试覆盖率要求

### 5.1 覆盖率目标
| 类型 | 目标 | 说明 |
|------|------|------|
| 行覆盖率 | ≥80% | 所有可执行代码 |
| 函数覆盖率 | ≥80% | 所有非静态函数 |
| 分支覆盖率 | ≥70% | 条件分支 |
| 关键路径 | 100% | ACL 匹配、Session 查找、IPC |

### 5.2 运行覆盖率
```bash
# C 单元测试（使用 gcov/lcov）
gcc -fprofile-arcs -ftest-coverage test_acl.c acl.c -o test_acl
./test_acl
gcov test_acl.c
lcov --capture --directory . --output coverage.info

# 前端覆盖率
npm run test -- --coverage
```

---

## 6. 运行测试

### 6.1 构建并运行测试
```bash
# 完整构建并运行测试
meson setup build
ninja -C build
ninja -C build test

# 运行特定模块测试
./build/dataplane/test_acl
./build/dataplane/test_session
./build/dataplane/test_route

# 前端测试带覆盖率
cd web/frontend
npm install
npm run test -- --coverage
```

### 6.2 测试开发工作流
```bash
# 修改代码后快速重新构建单个测试
ninja -C build dataplane/test_acl.p/test_acl.c.o
ninja -C build test_acl
./build/dataplane/test_acl

# 前端热重载测试
npm run test -- --watch
```

---

## 附录 A: 测试数据结构

### ACL 规则测试用例
```c
// 用例1: 拒绝 10.0.0.0/8 私有地址段
struct acl_rule deny_private_10 = {
    .src_ip = RTE_IPV4(10, 0, 0, 0),
    .src_mask = RTE_IPV4(255, 0, 0, 0),
    .allow = 0,
};

// 用例2: 拒绝 172.16.0.0/12 私有地址段
struct acl_rule deny_private_172 = {
    .src_ip = RTE_IPV4(172, 16, 0, 0),
    .src_mask = RTE_IPV4(255, 240, 0, 0),
    .allow = 0,
};

// 用例3: 拒绝 192.168.0.0/16 私有地址段
struct acl_rule deny_private_192 = {
    .src_ip = RTE_IPV4(192, 168, 0, 0),
    .src_mask = RTE_IPV4(255, 255, 0, 0),
    .allow = 0,
};

// 用例4: 允许 HTTP (端口80)
struct acl_rule allow_http = {
    .proto = IPPROTO_TCP,
    .dst_port_min = 80,
    .dst_port_max = 80,
    .match_ports = 1,
    .allow = 1,
};

// 用例5: 允许 HTTPS (端口443)
struct acl_rule allow_https = {
    .proto = IPPROTO_TCP,
    .dst_port_min = 443,
    .dst_port_max = 443,
    .match_ports = 1,
    .allow = 1,
};

// 用例6: 默认允许所有
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

### Session Key 测试用例
```c
// TCP Web 会话
struct session_key tcp_web = {
    .src_ip = RTE_IPV4(192, 168, 1, 100),
    .dst_ip = RTE_IPV4(10, 0, 0, 1),
    .src_port = 12345,
    .dst_port = 80,
    .proto = IPPROTO_TCP,
};

// TCP HTTPS 会话
struct session_key tcp_https = {
    .src_ip = RTE_IPV4(192, 168, 1, 100),
    .dst_ip = RTE_IPV4(10, 0, 0, 1),
    .src_port = 54321,
    .dst_port = 443,
    .proto = IPPROTO_TCP,
};

// UDP DNS 会话
struct session_key udp_dns = {
    .src_ip = RTE_IPV4(192, 168, 1, 100),
    .dst_ip = RTE_IPV4(8, 8, 8, 8),
    .src_port = 54321,
    .dst_port = 53,
    .proto = IPPROTO_UDP,
};
```

---

## 附录 B: 推荐测试工具

| 工具 | 用途 |
|------|------|
| Check Framework | C 单元测试框架 |
| cmocka | C Mocking 框架 |
| gcov/lcov | 覆盖率工具 |
| Vitest | Vue.js 测试框架 |
| @vue/test-utils | Vue 组件测试 |
| MSW | API Mock 工具 (Mock Service Worker) |
