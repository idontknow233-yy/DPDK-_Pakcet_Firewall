## 框架选择
- 采用 Vue 3 + Vite + TypeScript，轻量、生态成熟、中文资料丰富，适合快速构建管理面界面
- 组件库选用 Element Plus，图表选用 ECharts，状态管理用 Pinia，HTTP 用 Axios
- 理由：上手快、构建速度快、适合仪表盘+表单型管理界面；TypeScript 保持代码健壮性

## 目录与结构
- 新增 web/frontend：前端工程（Vite 项目）
- 新增 web/backend（可选）：网关服务，将现有控制面 CLI 转换为 REST 接口
- 前端结构：
  - src/app：应用入口、路由、状态
  - src/pages：Dashboard、ACL、Sessions、Ports
  - src/components：表格、表单、统计卡片
  - src/services：抽象 API 适配层（后端基地址、类型定义、拦截器）
  - src/assets：样式与图标

## 页面与功能
- Dashboard：端口状态、吞吐统计、规则数量、版本号
- ACL 管理：
  - 列表：读取规则快照（count+version+rules）
  - 新增：allow/deny、IP/掩码、协议、端口范围
  - 删除：按索引删除
  - 清空：一键清空规则
- 会话视图（预留）：展示连接数、协议分布
- 端口状态：链路、速率、混杂模式开关展示

## API 适配
- 后端 REST 规划（可选实现于 web/backend，初期可用 mock 适配）：
  - GET /api/acl → 返回 {version, count, rules[]}
  - POST /api/acl → 新增规则
  - DELETE /api/acl/:index → 删除规则
  - DELETE /api/acl → 清空规则
- 适配现有控制面：
  - 初期：backend 通过 TCP 8086 调用 CLI，解析输出（兼容当前协议）
  - 后续：在控制面 secondary 内嵌轻量 HTTP 服务器，直接通过 rte_ring 与共享快照响应

## 实施步骤
1. 前端初始化：在 web/frontend 创建 Vite + Vue3 + TS 项目，集成 Element Plus、ECharts、Pinia、Axios
2. 路由与布局：基础布局（侧边栏/顶部栏），配置页面路由
3. API 适配层：统一封装 GET/POST/DELETE，基地址配置、错误处理、类型定义
4. ACL 页面：
   - 表格展示规则（索引、src/dst、proto、端口范围、动作）
   - 表单新增规则（校验）
   - 删除/清空操作与反馈
5. Dashboard：端口与统计卡片，版本与规则数量
6. 构建脚本：添加 dev、build、preview 命令；环境变量区分开发/生产
7. 可选 backend：用 Python FastAPI 或 Go 轻量服务实现 CLI → REST 网关，先行跑通 ACL 四个接口

## 验证与交付
- 前端：本地开发启动，联通 mock 或 backend，完成 ACL CRUD 验证
- 后端（可选）：联通运行中的控制面 secondary，端到端验证规则增删改查与版本递增
- 交付：web/frontend 完整项目、README 使用说明、环境变量模板、API 文档（前端需要的字段）

## 风险与兼容
- 当前控制面是 CLI 文本协议，前端直连不可行，需要后端网关或在控制面集成 HTTP；计划先用网关降低改动风险
- 依赖管理：前端采用本地 node 环境，不影响 DPDK 数据面；后端独立进程，避免与 EAL 冲突

请确认以上方案，确认后我将初始化前端工程（web/frontend），并给出基础页面、API 适配与运行脚本；若选择后端网关，我将同步创建 web/backend 并实现 ACL 四个接口。