#!/bin/bash

# 脚本所在目录
SCRIPT_DIR=$(dirname "$(readlink -f "$0")")
PROJECT_ROOT=$(dirname "$SCRIPT_DIR")
LOG_DIR="$PROJECT_ROOT/log"

# 创建日志目录
mkdir -p "$LOG_DIR"

# 路径定义
DPDK_PATH="/home/yy/dpdk/dpdk-stable-24.11.4"
# 设置 LD_LIBRARY_PATH 以便找到 DPDK 动态库
export LD_LIBRARY_PATH=$DPDK_PATH/dpdklib/lib/x86_64-linux-gnu:$LD_LIBRARY_PATH

BUILD_DIR="$PROJECT_ROOT/build"
WEB_BACKEND_DIR="$PROJECT_ROOT/web/backend"
WEB_FRONTEND_DIR="$PROJECT_ROOT/web/frontend"

# 日志文件
LOG_DATAPLANE="$LOG_DIR/dataplane.log"
LOG_CONTROLPLANE="$LOG_DIR/controlplane.log"
LOG_BACKEND="$LOG_DIR/backend.log"
LOG_FRONTEND="$LOG_DIR/frontend.log"

# 清理历史日志（可选，为了让 tail 更清晰）
# > "$LOG_DATAPLANE"; > "$LOG_CONTROLPLANE"; > "$LOG_BACKEND"; > "$LOG_FRONTEND"

printf "=== 正在启动 DPDK Packet Firewall ===\n"
printf "日志目录: %s\n\n" "$LOG_DIR"

# 1. 绑定网卡
printf "[1/5] 绑定网卡...\n"
if ! $DPDK_PATH/usertools/dpdk-devbind.py -s | grep -q "0000:02:05.0.*drv=vfio-pci"; then
    sudo $DPDK_PATH/usertools/dpdk-devbind.py --noiommu-mode -b vfio-pci 0000:02:05.0 0000:02:06.0 0000:02:07.0 0000:02:08.0
    printf "  - 网卡已绑定到 vfio-pci\n"
else
    printf "  - 网卡已处于绑定状态\n"
fi

# 2. 启动数据面
printf "[2/5] 启动数据面...\n"
sudo LD_LIBRARY_PATH=$LD_LIBRARY_PATH nohup $BUILD_DIR/dataplane/dpdk_packet_firewall -l 0-3 -n 4 --proc-type=primary --file-prefix=fw -- -p 0x3 -P > "$LOG_DATAPLANE" 2>&1 &
DP_PID=$!
printf "  - 数据面 PID: %s\n" "$DP_PID"
printf "  - 日志: %s\n" "$LOG_DATAPLANE"
sleep 5

# 3. 启动控制面
printf "[3/5] 启动控制面...\n"
sudo LD_LIBRARY_PATH=$LD_LIBRARY_PATH nohup $BUILD_DIR/controlplane/control_plane -l 0-3 -n 4 --proc-type=secondary --file-prefix=fw -- --cli-host 127.0.0.1 --cli-port 8086 > "$LOG_CONTROLPLANE" 2>&1 &
CP_PID=$!
printf "  - 控制面 PID: %s\n" "$CP_PID"
printf "  - 日志: %s\n" "$LOG_CONTROLPLANE"
sleep 2

# 4. 启动后端
printf "[4/5] 启动后端...\n"
(cd "$WEB_BACKEND_DIR" && CLI_HOST=127.0.0.1 CLI_PORT=8086 HTTP_ADDR=:9000 nohup go run ./cmd/server > "$LOG_BACKEND" 2>&1 &)
BE_PID=$!
# 注意: go run 可能会启动子进程，这里记录的是 shell 的 PID，但足够识别
printf "  - 后端 PID: %s\n" "$BE_PID"
printf "  - 日志: %s\n" "$LOG_BACKEND"
sleep 2

# 5. 启动前端
printf "[5/5] 启动前端...\n"
(cd "$WEB_FRONTEND_DIR" && nohup npm run dev > "$LOG_FRONTEND" 2>&1 &)
FE_PID=$!
printf "  - 前端 PID: %s\n" "$FE_PID"
printf "  - 日志: %s\n" "$LOG_FRONTEND"

printf "\n=== 所有服务启动尝试完成 ===\n"
printf "请检查日志文件确认运行状态:\n"
printf "  tail -f %s/*.log\n" "$LOG_DIR"
