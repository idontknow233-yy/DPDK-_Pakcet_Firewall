## 根因定位
- 控制面 CLI 未将共享内存区指针赋值：在 memzone 查找到 ACL_SHARED_CFG_NAME 后，未执行 `acl_shared_cfg = mz->addr`，导致 `read_shared_rules()` 总是失败并输出 “ACL list failed”。
- 协议当前为：数据面仅发送列表头（count/version），控制面据此从共享快照读取并打印规则；因此共享指针未初始化会导致列表始终失败。

## 代码修改
- 在控制面初始化中完成共享指针赋值：
  - 位置：控制面入口 [acl_cli.c](file:///home/yy/DPDK_Packet_Firewall/controlplane/cli/acl_cli.c#L480-L526)
  - 变更：在 `rte_memzone_lookup(ACL_SHARED_CFG_NAME)` 成功后执行 `acl_shared_cfg = (struct acl_shared_cfg *)mz->addr;`
- 增强错误提示：
  - `acl list` 失败时区分是 ring 交互失败还是共享快照读取失败，给予更明确的提示（不改协议，仅改提示）。
- 保持现有数据面行为：
  - 数据面只发送头信息（count/version），不再逐条发 item，避免丢包导致不一致；列表输出完全来自共享快照。

## 运行与验证
- 启动顺序：先启动数据面 Primary，再启动控制面 Secondary（自动判定 secondary 或显式 `--proc-type=secondary`）。
- 验证步骤：
  - 执行 `acl list` 应输出规则数量与版本；默认包含 “deny 10.0.0.0/8” 与 “allow any”。
  - 执行 `acl add allow 192.168.1.0/24 10.0.0.1/32 6 0 65535 80 80` 后再次 `acl list`，应出现新增条目且版本递增。

## 兼容与风险
- 改动仅在控制面赋值与提示，不影响数据面路径；协议与共享结构保持一致。
- 若仍失败，优先检查：
  - Primary 是否已创建 rings 与 memzone（参考 [main.c](file:///home/yy/DPDK_Packet_Firewall/dataplane/main.c#L140-L180, file:///home/yy/DPDK_Packet_Firewall/dataplane/main.c#L300-L360) 的 `acl_ipc_init`）。
  - 两进程是否共享相同 DPDK hugepage 配置与 EAL 参数。

请确认以上计划，我将按此修改并进行端到端验证。