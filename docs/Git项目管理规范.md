# 基于DPDK高性能防火墙毕业设计 - Git 项目管理全规范（完整版）
## ✅ 规范前言 & 适用范围
本规范为你的**DPDK双栈高性能防火墙毕设项目**量身定制，完全适配你的项目特性：单人开发、分4阶段迭代、包含**C/C++核心代码+毕设文档+项目资源+配置脚本+测试用例+Web前端代码**等多类型文件；兼顾**规范性和实用性**，无冗余复杂规则，适合毕业设计场景，同时足够专业，答辩时展示该规范能体现你的工程化素养，**加分明显**。

管理范围：Git仓库纳入项目**所有相关资产**，具体包含：
1. 项目核心代码：DPDK数据面/控制面C/C++源码、头文件、编译脚本(meson/ninja/makefile)；
2. 项目文档：毕业设计论文（md/word/pdf）、需求分析、设计文档、测试报告、环境部署手册；
3. 项目资源：DPDK配置文件、规则集模板、测试流量脚本、性能测试数据、PPT答辩稿；
4. 附属代码：Web前端(Vue/React)、后端API(Python/Go)、CLI命令行相关代码；
5. 其他：gitignore、README、CHANGELOG等仓库基础文件。

规范核心原则：**单人开发、清晰溯源、分支极简、提交可读、版本可控**，所有规则均为「必遵+易用」，无多余约束。

---

## 一、 核心分支管理规范（重中之重，优先遵守）
### ✅ 分支策略选型
采用 **「简化版GitFlow分支模型」**，专为**单人毕设开发**定制，摒弃复杂的GitFlow多角色分支，保留核心分支体系，兼顾「开发灵活性」和「版本稳定性」，完美适配你分**4个阶段开发、模块化迭代**的项目节奏，**这是本次毕设的最优分支方案**，没有之一。

### ✅ 分支分类 & 命名规范（严格遵守）
所有分支**全部小写、用中划线 `-` 分隔**，禁止大写/下划线/特殊字符，分支命名必须「见名知意」，共分为 **4类分支**，优先级从高到低：主分支 > 发布分支 > 功能分支 > 修复分支，**所有分支都有明确的创建来源和合并目标，禁止跨规则操作**。

#### 1. 【主分支】2个核心永久分支（仓库基石，永不删除）
> 主分支是仓库的核心，存放**稳定、可运行、经过测试**的代码，**禁止直接在主分支上编写代码/提交修改**，所有修改必须通过「功能分支/修复分支」合并到主分支，这是底线规则。
- **`main` 分支（主分支/生产分支）**
  - 作用：存放项目**最终稳定、可验收、可答辩**的代码版本，是项目的「黄金基线」；
  - 内容：代码无bug、功能完整、文档齐全、编译运行正常，完全符合你的毕设验收指标；
  - 规则：仅接受从 `develop` 分支或 `release/*` 分支的合并请求，**任何人（包括你）禁止直接push代码到main**；
  - 你的项目适配：毕设最终定稿的所有代码、最终版论文、最终版PPT，全部在该分支。

- **`develop` 分支（开发主分支/集成分支）**
  - 作用：存放项目**最新的开发完成、经过自测的代码**，是你日常开发的「主力分支」，也叫「集成分支」；
  - 内容：代码功能完整、无严重bug、可编译运行，是所有功能开发的基准；
  - 规则：仅接受从 `feature/*` 功能分支、`bugfix/*` 修复分支的合并，**禁止直接在develop上编写代码**；
  - 你的项目适配：你完成的所有功能模块（如IPv6解析、ACL规则引擎、连接跟踪），自测通过后都合并到该分支，该分支始终是「可运行的开发版」。

#### 2. 【功能分支】`feature/xxx` 临时分支（核心开发分支，最多用）
> 最核心的开发分支，**所有新功能、新模块的开发，必须在独立的feature分支完成**，完美适配你的「模块化开发」需求，是单人开发的核心。
- 命名规范：`feature/模块名-功能描述` 或 `feature/阶段名-核心任务`
- 创建来源：**必须从 `develop` 分支创建**
- 合并目标：开发完成+自测通过后，合并回 `develop` 分支
- 生命周期：功能开发完成、合并后**立即删除该分支**，不保留冗余分支
- 你的项目「专属贴合示例」（直接复制使用，完美对应你的项目方案）：
  ```
  feature/dataplane-ipv4-parse      # 数据面-实现IPv4协议解析
  feature/dataplane-ipv6-nd         # 数据面-实现IPv6邻居发现协议
  feature/conn-track-rtehash        # 核心功能-基于rte_hash实现连接跟踪
  feature/acl-rule-engine           # 核心功能-实现rte_acl规则匹配引擎
  feature/lpm-route-forward         # 核心功能-基于rte_lpm实现路由转发
  feature/control-cli-cmdline       # 控制面-实现rte_cmdline命令行控制台
  feature/web-api-rest              # 控制面-实现Web的REST API接口
  feature/docs-project-design       # 文档-编写项目总体设计文档
  ```
✅ 核心建议：一个feature分支只做「一个独立功能/一个模块」，比如不要在一个分支里同时开发「连接跟踪+ACL规则」，小粒度分支能让你的开发逻辑清晰，提交记录整洁，答辩时能清晰展示你的开发轨迹。

#### 3. 【修复分支】`bugfix/xxx` 临时分支（问题修复专用）
> 用于修复代码中的bug、文档中的错误、配置文件的问题等，分为「开发版修复」和「稳定版修复」，优先级高于feature分支。
- 命名规范：`bugfix/模块名-问题描述`
- 创建来源：
  - 修复develop分支的bug → 从`develop`创建；
  - 修复main分支的紧急bug → 从`main`创建；
- 合并目标：对应创建来源的分支（修develop合develop，修main合main）
- 生命周期：修复完成+自测通过后，**立即删除该分支**
- 你的项目示例：
  ```
  bugfix/dataplane-ipv6-checksum    # 修复数据面IPv6校验和计算错误
  bugfix/conn-track-timeout         # 修复连接跟踪表超时清理失效的bug
  bugfix/docs-acl-rule              # 修复文档中ACL规则描述错误
  bugfix/build-meson-config         # 修复meson编译配置文件的路径错误
  ```

#### 4. 【发布分支】`release/vx.y.z` 临时分支（里程碑版本专用，毕设加分项）
> 用于**项目阶段里程碑发布、版本固化、最终测试**，完美适配你的「4个阶段分阶段开发计划」，是毕设中体现「版本可控」的核心亮点，**强烈建议使用**。
- 命名规范：`release/v主版本号.次版本号.修订号` （版本号规则见下文）
- 创建来源：从 `develop` 分支创建，创建时代表「该阶段开发完成，进入测试验收阶段」
- 合并目标：测试通过后，**同时合并到 `develop` 和 `main` 分支**，并在main分支打对应的版本标签
- 生命周期：发布完成+合并后，**立即删除该分支**
- 你的项目「阶段专属示例」（完美对应你的4个开发阶段，直接用）：
  ```
  release/v0.1.0  # 第一阶段：基础数据面搭建完成，发布第一个里程碑版本
  release/v0.2.0  # 第二阶段：核心防火墙功能完成，发布第二个里程碑版本
  release/v0.3.0  # 第三阶段：控制面与Web管理系统完成，发布第三个里程碑版本
  release/v1.0.0  # 第四阶段：测试优化完成，毕设最终定稿，发布正式版（答辩版）
  ```

### ✅ 分支操作核心规则（必遵，共6条，无例外）
1. 所有开发工作，**必须在feature/bugfix分支进行**，禁止直接操作main/develop；
2. 新建分支前，必须先`git pull`对应源分支的最新代码，保证分支基线最新，减少合并冲突；
3. 功能分支开发完成后，**先本地自测通过（编译+运行+功能验证）**，再合并到develop；
4. 合并分支优先使用 `git merge --no-ff` （保留分支历史），拒绝`git rebase`（单人开发无必要，易搞乱历史）；
5. 分支合并后，**立即删除该临时分支**，仓库只保留main+develop两个永久分支；
6. 每天开发前，先`git pull`更新当前分支；每天开发结束，至少提交1次代码，保证代码不丢失。

---

## 二、 Commit 提交信息规范（核心规范，答辩加分重点）
> Commit信息是你项目的「开发日志」，是毕设答辩时**老师重点关注的细节**：清晰、规范、结构化的commit信息，能体现你的开发逻辑和严谨性，直接加分；混乱的commit信息（如`git commit -m "修改代码"`/`更新`）会减分。
> 本规范为**结构化、标准化的Commit信息格式**，专为你的防火墙毕设定制，**兼顾规范性和易用性**，所有规则都有贴合你项目的示例，直接套用即可，无学习成本。

### ✅ 核心原则
1. Commit信息**必须使用英文书写**（学术项目规范，毕设属于学术成果，英文更专业）；
2. 提交粒度：**小粒度、高频次提交** → 一个commit只做「一个独立的修改点」，比如「实现IPv4解析」「修复TCP连接超时bug」「补充ACL规则文档」，禁止一次性提交几百行代码+多个功能修改；
3. 提交内容：提交前必须检查，**只提交源码、文档、资源等必要文件**，禁止提交编译产物、日志、临时文件、IDE缓存等（通过.gitignore过滤）；
4. 每次提交前，必须先本地编译运行，确保无语法错误、无运行时panic，再执行commit。

### ✅ 标准化 Commit 格式（严格遵守，必填+可选）
采用行业通用的 **「<类型>(<模块>): <简短描述>」** 格式，**单行式为主，按需补充多行正文**，所有内容一行写完，简洁清晰，适配单人开发，格式如下（`[]`内为可选，`()`内为必填）：
```git
<type>(<scope>): <subject>
[
<body>
]
[
<footer>
]
```
#### 1. 核心必填项（99%的提交只用这一行足够）
##### ✔️ `<type>`：提交类型（必须是以下9个关键字之一，全部小写）
**专为你的DPDK防火墙项目定制的提交类型**，完美覆盖你的「代码开发、文档编写、测试优化、编译构建」等所有场景，无多余类型，直接用：
- `feat`：新增功能/模块/特性 ✅【用的最多】，比如实现连接跟踪、新增IPv6转发、开发Web API；
- `fix`：修复bug/错误，比如修复IP分片重组的内存泄漏、修复ACL规则匹配错误；
- `docs`：仅修改文档，比如更新毕设论文、补充模块设计文档、修改README；
- `perf`：性能优化，比如开启大页内存、调优收发包批处理大小、优化rte_hash查询效率 ✅【你的项目重点】；
- `refactor`：代码重构（无功能增删、无bug修复），比如优化代码结构、抽离公共函数、变量名规范化；
- `test`：新增/修改测试用例、测试脚本，比如新增pktgen-dpdk测试脚本、补充单元测试；
- `build`：仅修改编译构建相关文件，比如修改meson.build、makefile、DPDK环境配置；
- `chore`：杂项修改（不影响代码运行、不影响功能），比如修改.gitignore、整理资源文件、更新CHANGELOG；
- `style`：代码格式调整（无逻辑变更），比如缩进、空格、注释规范、代码对齐，不影响编译运行。

##### ✔️ `<scope>`：提交所属模块（必须填写，项目专属模块名，全部小写）
**重中之重！专为你的项目「量身定制的模块名」**，完美对应你在毕设方案里的「核心功能模块分解」，**直接复制使用，无需自己思考**，这是本规范的核心亮点，每个commit都能精准对应项目模块，答辩时一目了然：
> 所有模块名全部取自你的毕设方案，1:1匹配，共9个核心模块：
- `dataplane`：数据面核心（收发包、协议解析、ACL、LPM路由、连接跟踪、多核扩展）✅【核心模块】
- `controlplane`：控制面管理（CLI控制台、Web API、配置管理、进程间通信）✅【核心模块】
- `protocol`：网络协议模块（ARP、ND、ICMP/ICMPv6、TCP/UDP协议处理）
- `ipc`：进程间通信（rte_ring、共享内存、rte_metrics统计）
- `web`：Web前端/后端（Vue/React界面、REST API服务）
- `utils`：工具类/公共函数（日志、内存池封装、公共宏定义）
- `docs`：项目文档（毕设论文、设计文档、测试报告、PPT）
- `resource`：项目资源（规则模板、测试脚本、DPDK配置文件）
- `project`：项目整体（README、CHANGELOG、gitignore等根目录文件）

##### ✔️ `<subject>`：简短描述（必须填写，50字符以内）
- 英文书写，首字母**小写**，结尾**无句号**；
- 简洁明了，一句话说明「做了什么」，比如 `implement tcp connection track with rte_hash`、`fix ipv6 nd multicast address error`；
- 禁止模糊描述，比如 `update code`、`modify data` 这类无意义的描述。

#### 2. 可选补充项（仅在需要时使用）
- `<body>`：多行正文，详细描述修改的原因、实现思路、优化点，比如「开启2MB大页内存，减少TLB Miss，提升mbuf访问效率，实测小包吞吐提升15%」；
- `<footer>`：备注信息，比如关联的bug编号、里程碑版本、待办事项，毕设单人开发基本用不到。

### ✅ 完美贴合你的项目 - Commit 示例（直接复制使用，超全）
**所有示例均为你的项目真实场景，99%的提交都能参考这些例子，直接套用即可**，这是你最需要的内容：
```git
# 新增功能
feat(dataplane): implement ipv4/ipv6 dual stack packet parse
feat(dataplane): add tcp/udp connection track with rte_hash and rte_timer
feat(controlplane): develop cli console with rte_cmdline library
feat(web): add rest api for acl rule add/delete/update

# 修复bug
fix(dataplane): fix ip frag reassembly memory leak bug
fix(protocol): fix arp table timeout clean error
fix(acl): fix rule priority match conflict issue

# 文档修改
docs(project): update project design document with performance index
docs(dataplane): add comment for rte_lpm route forward code
docs(thesis): supplement the acl engine chapter in graduation thesis

# 性能优化 ✅ 重点
perf(dataplane): optimize rx/tx burst size to 64 packets per batch
perf(dataplane): enable hugepage 2MB to reduce tlb miss
perf(dataplane): optimize rte_hash lookup speed with cache alignment

# 代码重构
refactor(dataplane): extract common ip parse function to utils
refactor(controlplane): normalize variable name in cli module

# 编译构建
build(project): update meson.build to link rte_acl library
build(project): modify makefile to support dpdk 24.11 version

# 杂项
chore(project): update .gitignore to filter dpdk build output
chore(resource): organize pktgen test scripts to resource folder
```

---

## 三、 版本号规范 & Tag 标签管理（毕设里程碑，必做，加分项）
### ✅ 标准化版本号规则
采用 **语义化版本号 Semantic Versioning (SemVer)**，格式：`vX.Y.Z`，完美适配你的「4阶段分阶段开发计划」，版本号与你的开发阶段强绑定，**答辩时展示版本标签，能体现你的项目进度管控能力，直接加分**。
- `X`：主版本号（Major）→ 重大重构/功能大改，毕设项目固定为`1`；
- `Y`：次版本号（Minor）→ 完成一个阶段的核心开发，新增大量功能，你的项目用这个区分4个阶段 ✅；
- `Z`：修订号（Patch）→ 修复bug、小优化、文档更新，按需递增；
- 版本号递增规则：**只增不减**，禁止回滚版本号。

### ✅ 你的项目「专属版本号规划」（直接套用，完美匹配开发阶段）
```
v0.1.0  → 第一阶段完成：基础数据面搭建，支持IPv4/IPv6基础转发
v0.2.0  → 第二阶段完成：核心防火墙功能，连接跟踪+ACL+路由转发全实现
v0.3.0  → 第三阶段完成：控制面+Web管理系统，CLI+REST API+可视化界面全实现
v1.0.0  → 第四阶段完成：测试优化+文档定稿，毕设最终答辩版（正式版）
```

### ✅ Tag 标签管理规则
- 每个里程碑版本，在`main`分支上打一个对应的Tag标签，命令：`git tag -a v1.0.0 -m "release: 毕设最终答辩版，完成所有功能与优化"`；
- 标签永久保留，用于回溯版本，答辩时可通过`git tag`展示你的项目里程碑；
- 打标签前，必须确保该版本代码编译正常、功能完整、无bug，是稳定版本。

---

## 三、 专属定制 .gitignore 文件（必用，直接复制到项目根目录）
**为你的DPDK高性能防火墙项目「100%精准定制」的.gitignore文件**，覆盖所有不需要提交的文件，避免仓库中出现编译产物、日志、临时文件等冗余内容，**直接复制到项目根目录，命名为 .gitignore**，一劳永逸，无需修改。
```gitignore
# ==================== 通用编译产物 ====================
*.o
*.a
*.so
*.ko
*.out
*.exe
build/
_build/
bin/
obj/
lib/

# ==================== DPDK 专属 ====================
dpdk-build/
dpdk-install/
*.dpdk
rte_*
hugepage/
dpdk.log

# ==================== C/C++ 通用 ====================
*.d
*.i
*.s
*.pdb
*.gcno
*.gcda
coverage/

# ==================== 日志与临时文件 ====================
*.log
*.tmp
*.bak
*.swp
*.swo
*~
.DS_Store
Thumbs.db

# ==================== IDE/编辑器缓存 ====================
.vscode/
.idea/
.cproject
.project
.settings/
cmake-build-*/

# ==================== Web 前端/后端 ====================
node_modules/
dist/
build/
npm-debug.log
yarn-error.log
venv/
env/
__pycache__/
*.pyc
*.pyo

# ==================== 毕设临时文件 ====================
*.tmp.doc
*.tmp.pdf
*.bak.ppt
test_data/
temp_script/
```

---

## 四、 Git 操作最佳实践（毕设专属，避坑+高效，必看）
### ✅ 日常开发流程（每日必做，固定步骤，无错误）
完美适配你的单人开发模式，每天开发按这个流程走，不会出现分支混乱、代码丢失、合并冲突等问题，步骤如下：
```bash
# 1. 打开终端，进入项目目录，切换到develop分支，拉取最新代码
git checkout develop
git pull origin develop

# 2. 创建新的功能分支，开始开发
git checkout -b feature/dataplane-ipv6-route

# 3. 开发代码/编写文档，中途可多次add+commit，小粒度提交
git add 文件名/目录名
git commit -m "feat(dataplane): implement ipv6 route forward with rte_lpm6"

# 4. 功能开发完成，自测通过后，切换回develop，拉取最新代码
git checkout develop
git pull origin develop

# 5. 合并功能分支到develop，保留分支历史
git merge --no-ff feature/dataplane-ipv6-route -m "merge: add ipv6 route forward feature"

# 6. 推送develop分支到远程仓库，删除本地临时分支
git push origin develop
git branch -d feature/dataplane-ipv6-route
```

### ✅ 毕设关键节点 Git 操作（4个阶段完成后必做）
1. 每个阶段开发完成后，创建`release/vx.y.z`分支，进行全量测试；
2. 测试通过后，将release分支合并到`develop`和`main`分支；
3. 在`main`分支打对应的Tag标签，推送标签到远程仓库；
4. 删除release分支，进入下一阶段开发。

### ✅ 避坑指南（Git新手最常犯的5个错误，全部避开）
1. ❌ 禁止直接在main/develop分支写代码：会导致稳定分支被污染，回滚困难；
2. ❌ 禁止一次性提交大量代码：会导致commit记录混乱，无法溯源，答辩时无法展示开发轨迹；
3. ❌ 禁止提交编译产物/日志：仓库体积会越来越大，通过.gitignore过滤即可；
4. ❌ 禁止在未编译测试的情况下提交：会导致仓库中存在错误代码，影响后续开发；
5. ❌ 禁止使用`git rebase`：单人开发无必要，极易搞乱提交历史，用`git merge --no-ff`即可。

---

## 五、 总结（规范核心精华，收藏备查）
你的DPDK高性能防火墙毕设项目，Git规范的核心只有3点，记住即可：
1. **分支清晰**：main(定稿) + develop(开发) + feature/bugfix(临时)，一个功能一个分支；
2. **提交规范**：`类型(模块): 描述`，英文书写，小粒度提交，清晰可读；
3. **版本可控**：分阶段打Tag，里程碑明确，代码可追溯。

这套规范足够专业，完全适配你的毕设项目，答辩时如果老师问到「项目如何管理版本」，你展示这套规范+提交记录+版本标签，绝对能让老师眼前一亮，成为加分项。

最后，祝你开发顺利，毕设圆满完成！🚀