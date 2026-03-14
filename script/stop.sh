#!/bin/bash

# 脚本所在目录
SCRIPT_DIR=$(dirname "$(readlink -f "$0")")
PROJECT_ROOT=$(dirname "$SCRIPT_DIR")

echo "=== 停止 DPDK Packet Firewall ==="

# 1. 停止前端
echo "[1/4] 停止前端..."
if pkill -f "vite" >/dev/null 2>&1; then
    echo "  - 前端已停止"
else
    echo "  - 前端未运行"
fi

# 2. 停止后端
echo "[2/4] 停止后端..."
if pkill -f "go run ./cmd/server" >/dev/null 2>&1; then
    echo "  - 后端已停止"
else
    # 尝试直接 kill 编译后的二进制 (如果用户直接运行二进制)
    if pkill -f "dpdk-packet-firewall-web-backend" >/dev/null 2>&1; then
        echo "  - 后端(二进制)已停止"
    else
        echo "  - 后端未运行"
    fi
fi

# 3. 停止控制面
echo "[3/4] 停止控制面..."
if sudo pkill -f "control_plane" >/dev/null 2>&1; then
    echo "  - 控制面已停止"
else
    echo "  - 控制面未运行"
fi

# 4. 停止数据面
echo "[4/4] 停止数据面..."
if sudo pkill -f "dpdk_packet_firewall" >/dev/null 2>&1; then
    echo "  - 数据面已停止"
else
    echo "  - 数据面未运行"
fi

# 5. 清理残留
echo "清理残留进程..."
# 有时候 pkill -f 可能漏掉某些特定参数启动的进程，这里再次尝试清理
pkill -f "node" >/dev/null 2>&1 || true
pkill -f "server" >/dev/null 2>&1 || true

echo ""
echo "=== 所有服务已停止 ==="
