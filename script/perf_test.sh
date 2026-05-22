#!/usr/bin/env bash
set -euo pipefail

DPI=/home/yy/dpdk/dpdk-stable-24.11.4/usertools/dpdk-devbind.py
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
FW_BIN="${ROOT_DIR}/build/dataplane/dpdk_packet_firewall"
LOG_FILE="${ROOT_DIR}/log/perf_test.log"
DPDK_LIB=/home/yy/dpdk/dpdk-stable-24.11.4/dpdklib/lib/x86_64-linux-gnu

# 设置 DPDK 24.11 库路径（脚本以 sudo 运行时生效）
export LD_LIBRARY_PATH="${DPDK_LIB}:${LD_LIBRARY_PATH:-}"

RED='\033[0;31m'
GRN='\033[0;32m'
YEL='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m'

log()  { echo -e "${CYAN}[$(date +%T)]${NC} $*"; }
ok()   { echo -e "${GRN}[OK]${NC} $*"; }
warn() { echo -e "${YEL}[WARN]${NC} $*"; }
err()  { echo -e "${RED}[ERR]${NC} $*"; }

# PCI 地址: VMXNET3 NICs
PORT0_PCI="0000:03:00.0"   # ens160 (Port 0)
PORT1_PCI="0000:0b:00.0"   # ens192 (Port 1)
PORT0_MAC=""
PORT1_MAC=""
TRAFFIC_NIC="ens224"
TRAFFIC_MAC=""

cleanup() {
    log "清理..."
    if [ -f /tmp/perf_test_fw.pid ]; then
        sudo kill -9 "$(cat /tmp/perf_test_fw.pid)" 2>/dev/null || true
    fi
    if [ -f /tmp/perf_test_tg.pid ]; then
        sudo kill -9 "$(cat /tmp/perf_test_tg.pid)" 2>/dev/null || true
    fi
    sudo python3 "$DPI" --noiommu-mode --bind=e1000 "$PORT0_PCI" 2>/dev/null || \
        sudo python3 "$DPI" --noiommu-mode --bind=vmxnet3 "$PORT0_PCI" 2>/dev/null || true
    sudo python3 "$DPI" --noiommu-mode --bind=e1000 "$PORT1_PCI" 2>/dev/null || \
        sudo python3 "$DPI" --noiommu-mode --bind=vmxnet3 "$PORT1_PCI" 2>/dev/null || true
    sudo ip link set ens160 up 2>/dev/null || true
    sudo ip link set ens192 up 2>/dev/null || true
    rm -f /tmp/perf_test_fw.pid /tmp/perf_test_tg.pid /tmp/perf_test_tg_result
}
trap cleanup EXIT

# ===================== Step 1: 获取 MAC 地址 =====================
log "Step 1: 获取网卡 MAC 地址"
PORT0_MAC=$(cat /sys/class/net/ens160/address 2>/dev/null || echo "00:0c:29:fb:49:f3")
PORT1_MAC=$(cat /sys/class/net/ens192/address 2>/dev/null || echo "00:0c:29:fb:49:fd")
TRAFFIC_MAC=$(cat /sys/class/net/${TRAFFIC_NIC}/address 2>/dev/null)
ok "Port 0 MAC: $PORT0_MAC"
ok "Port 1 MAC: $PORT1_MAC"
ok "Traffic NIC ($TRAFFIC_NIC) MAC: $TRAFFIC_MAC"

# ===================== Step 2: 停旧进程 =====================
log "Step 2: 停止旧进程"
pgrep -f "[d]pdk_packet_firewall" | xargs -r sudo kill -9 2>/dev/null || true
pgrep -f "[c]ontrol_plane" | xargs -r sudo kill -9 2>/dev/null || true
sleep 1
ok "旧进程已清理"

# ===================== Step 3: 绑定 NIC 到 DPDK =====================
log "Step 3: 绑定 VMXNET3 网卡到 vfio-pci"
sudo ip link set ens160 down 2>/dev/null || true
sudo ip link set ens192 down 2>/dev/null || true

# 确保 vfio noiommu 模式开启
if [ "$(cat /sys/module/vfio/parameters/enable_unsafe_noiommu_mode 2>/dev/null)" != "Y" ]; then
    echo 1 | sudo tee /sys/module/vfio/parameters/enable_unsafe_noiommu_mode >/dev/null
    ok "已开启 vfio unsafe_noiommu_mode"
fi

sudo python3 "$DPI" --noiommu-mode --bind=vfio-pci "$PORT0_PCI"
sudo python3 "$DPI" --noiommu-mode --bind=vfio-pci "$PORT1_PCI"
echo ""
sudo python3 "$DPI" --status 2>/dev/null | grep -E "Network|03:00|0b:00"
ok "NIC 绑定完成"

# ===================== Step 4: 配置流量发送接口 =====================
log "Step 4: 配置流量发送接口 ($TRAFFIC_NIC)"
sudo ip link set "$TRAFFIC_NIC" up 2>/dev/null || true
# 确保流量口没有 IP 干扰
sudo ip addr flush dev "$TRAFFIC_NIC" 2>/dev/null || true
ok "${TRAFFIC_NIC} 已就绪"

# ===================== Step 5: 启动数据面 =====================
log "Step 5: 启动数据面防火墙"
echo ""
echo "  [测试参数]"
echo "  - 模式:     L2 转发 (无 ACL / 无路由)"
echo "  - CPU:      lcore 0-1"
echo "  - 端口:     0x3 (Port 0 <-> Port 1)"
echo "  - 混杂模式:  OFF (避免转发环路)"
echo "  - 统计周期: 5 秒"
echo "  - 日志文件: ${LOG_FILE}"
echo ""

stdbuf -oL "$FW_BIN" \
    -l 0-1 -n 4 --proc-type=primary -- \
    -p 0x3 -T 5 >"${LOG_FILE}" 2>&1 &
FW_PID=$!
FW_PID=$!
echo "$FW_PID" > /tmp/perf_test_fw.pid

# 等数据面初始化完成
log "等待数据面初始化..."
for i in $(seq 1 30); do
    if grep -q "entering main loop" "${LOG_FILE}" 2>/dev/null; then
        ok "数据面已启动 (PID=$FW_PID)"
        break
    fi
    if ! kill -0 "$FW_PID" 2>/dev/null; then
        err "数据面启动失败，查看日志:"
        tail -30 "${LOG_FILE}"
        exit 1
    fi
    sleep 1
done

sleep 2
echo ""

# ===================== Step 6: 流量生成 =====================
log "Step 6: 启动流量生成器"
echo ""

cat > /tmp/traffic_gen.py << 'PYEOF'
import socket, time, struct, sys, threading

PORT0_MAC = sys.argv[1] if len(sys.argv) > 1 else "00:0c:29:fb:49:f3"
TRAFFIC_MAC = sys.argv[2] if len(sys.argv) > 2 else "00:0c:29:fb:49:07"
IFACE = sys.argv[3] if len(sys.argv) > 3 else "ens224"
PKT_SIZE = int(sys.argv[4]) if len(sys.argv) > 4 else 1400
DURATION = int(sys.argv[5]) if len(sys.argv) > 5 else 30
NTHREADS = 4

def mac_to_bytes(mac_str):
    return bytes(int(b, 16) for b in mac_str.split(':'))

dst_mac = mac_to_bytes(PORT0_MAC)
src_mac = mac_to_bytes(TRAFFIC_MAC)
ethertype = struct.pack('!H', 0x0800)

pkt_size = max(PKT_SIZE, 64)
payload_size = pkt_size - 14 - 20 - 8

ip_ver_ihl = 0x45
ip_len = 20 + 8 + payload_size
ip_src = struct.pack('!I', 0x0a000001)
ip_dst = struct.pack('!I', 0x0a000002)

def ip_cksum(hdr):
    s = sum((hdr[i] << 8) + hdr[i+1] for i in range(0, len(hdr), 2))
    while s >> 16: s = (s & 0xffff) + (s >> 16)
    return ~s & 0xffff

ip_hdr = struct.pack('!BBHHHBBH', 0x45, 0, ip_len, 0x1234, 0, 64, 17, 0)
ip_hdr = ip_hdr[:10] + struct.pack('!H', ip_cksum(ip_hdr)) + ip_src + ip_dst
udp_hdr = struct.pack('!HHHH', 12345, 80, 8 + payload_size, 0)
payload = b'\x00' * payload_size
frame = dst_mac + src_mac + ethertype + ip_hdr + udp_hdr + payload

total = [0] * NTHREADS
lock = threading.Lock()
running = [True]

def sender(tid):
    sock = socket.socket(socket.AF_PACKET, socket.SOCK_RAW)
    sock.bind((IFACE, 0))
    cnt = 0
    while running[0]:
        for _ in range(32):
            try:
                sock.send(frame)
                cnt += 1
            except:
                break
        if cnt % 10000 == 0:
            total[tid] = cnt
    total[tid] = cnt
    sock.close()

print(f"[TG] pkt_size={pkt_size}B threads={NTHREADS} duration={DURATION}s")
print(f"[TG] dst={PORT0_MAC} iface={IFACE}")

threads = [threading.Thread(target=sender, args=(i,)) for i in range(NTHREADS)]
start = time.time()
for t in threads:
    t.start()

last_print = start
last_cnt = 0
while time.time() - start < DURATION:
    time.sleep(2)
    elapsed = time.time() - start
    cur = sum(total)
    delta = cur - last_cnt
    pps = delta / (time.time() - last_print) if (time.time() - last_print) > 0 else 0
    mbps = delta * pkt_size * 8 / (time.time() - last_print) / 1e6 if (time.time() - last_print) > 0 else 0
    print(f"[TG] {cur:>10d} pkts | {elapsed:>5.1f}s | {pps:>10.0f} pps | {mbps:>7.1f} Mbps")
    last_cnt = cur
    last_print = time.time()

running[0] = False
for t in threads:
    t.join()

all_total = sum(total)
elapsed = time.time() - start
pps = all_total / elapsed if elapsed > 0 else 0
mbps = all_total * pkt_size * 8 / elapsed / 1e6 if elapsed > 0 else 0
print(f"\n[TG] ========== 发送完成 ==========")
print(f"[TG] 总包数:  {all_total}")
print(f"[TG] 总时长:  {elapsed:.1f}s")
print(f"[TG] 速率:    {pps:.0f} pps | {mbps:.1f} Mbps")

with open("/tmp/perf_test_tg_result", "w") as f:
    f.write(f"pkts={all_total}\n")
    f.write(f"elapsed={elapsed:.2f}\n")
    f.write(f"pps={pps:.0f}\n")
    f.write(f"mbps={mbps:.1f}\n")
    f.write(f"pkt_size={pkt_size}\n")
PYEOF

sudo python3 /tmp/traffic_gen.py "$PORT0_MAC" "$TRAFFIC_MAC" "$TRAFFIC_NIC" 1400 30 &
TG_PID=$!
echo "$TG_PID" > /tmp/perf_test_tg.pid
ok "流量生成器已启动 (PID=$TG_PID, 30 秒)"

# ===================== Step 7: 等待测试完成 =====================
log "Step 7: 测试运行中... (30秒，数据面每 5 秒输出统计)"
echo ""
echo "  [实时统计输出]"
echo "  ─────────────────────────────────────────────"
echo ""

# 监控数据面日志
prev_rx=0
prev_tx=0
prev_time=""

MULTI=1  # 发送端线程数

while kill -0 "$TG_PID" 2>/dev/null; do
    sleep 5
    
    # 快速解析 port stats
    total_rx=$(grep "Total packets received:" "${LOG_FILE}" | tail -1 | grep -oP '\d+')
    total_tx=$(grep "Total packets sent:" "${LOG_FILE}" | tail -1 | grep -oP '\d+')
    total_drop=$(grep "Total packets dropped:" "${LOG_FILE}" | tail -1 | grep -oP '\d+')
    
    if [ -n "$total_rx" ] && [ -n "$total_tx" ]; then
        delta_rx=$((total_rx - prev_rx))
        delta_tx=$((total_tx - prev_tx))
        pps_rx=$((delta_rx / 5))
        pps_tx=$((delta_tx / 5))
        mbps_rx=$(echo "scale=1; $delta_rx * 1400 * 8 / 5 / 1000000" | bc 2>/dev/null || echo "N/A")
        mbps_tx=$(echo "scale=1; $delta_tx * 1400 * 8 / 5 / 1000000" | bc 2>/dev/null || echo "N/A")
        echo -e "  [$(date +%T)] RX=$total_rx(+${delta_rx}) $pps_rx pps ${mbps_rx}Mbps | TX=$total_tx(+${delta_tx}) $pps_tx pps ${mbps_tx}Mbps | DROP=$total_drop"
        prev_rx=$total_rx
        prev_tx=$total_tx
    fi
done

wait "$TG_PID" || true

# ===================== Step 8: 最终统计 =====================
log "Step 8: 最终统计"
echo ""

# 读取 TG 结果
if [ -f /tmp/perf_test_tg_result ]; then
    echo "  ┌─── 流量生成器 (${TRAFFIC_NIC}) ───┐"
    while IFS='=' read -r k v; do
        case "$k" in
            pkts)   printf "  │ 发送包数:    %s\n" "$v" ;;
            pps)    printf "  │ 发送速率:    %s pps\n" "$v" ;;
            mbps)   printf "  │ 发送带宽:    %s Mbps\n" "$v" ;;
            pkt_size) printf "  │ 包大小:      %s bytes\n" "$v" ;;
            elapsed) printf "  │ 测试时长:    %s s\n" "$v" ;;
        esac
    done < /tmp/perf_test_tg_result
    echo "  └──────────────────────────┘"
fi

echo ""

# 读取 DPDK 最终统计
total_rx=$(grep "Total packets received:" "${LOG_FILE}" | tail -1 | grep -oP '\d+')
total_tx=$(grep "Total packets sent:" "${LOG_FILE}" | tail -1 | grep -oP '\d+')
total_drop=$(grep "Total packets dropped:" "${LOG_FILE}" | tail -1 | grep -oP '\d+')

echo "  ┌─── 数据面 (DPDK) ────────┐"
echo "  │ 接收包数:    ${total_rx:-N/A}"
echo "  │ 发送包数:    ${total_tx:-N/A}"
echo "  │ 丢包数:      ${total_drop:-N/A}"
echo "  └──────────────────────────┘"

echo ""
echo "  ┌─── 结果分析 ──────────────────────────────────┐"

# 计算转发率
if [ -n "$total_rx" ] && [ "$total_rx" -gt 0 ] 2>/dev/null; then
    tg_pkts=$(grep "^pkts=" /tmp/perf_test_tg_result 2>/dev/null | cut -d= -f2)
    tg_elapsed=$(grep "^elapsed=" /tmp/perf_test_tg_result 2>/dev/null | cut -d= -f2)
    
    if [ -n "$tg_pkts" ] && [ -n "$tg_elapsed" ]; then
        fw_pps=$(echo "scale=0; $total_rx / $tg_elapsed" | bc 2>/dev/null)
        tg_pps=$(echo "scale=0; $tg_pkts / $tg_elapsed" | bc 2>/dev/null)
        fw_mbps=$(echo "scale=1; $total_rx * 1400 * 8 / $tg_elapsed / 1000000" | bc 2>/dev/null)
        tg_mbps=$(echo "scale=1; $tg_pkts * 1400 * 8 / $tg_elapsed / 1000000" | bc 2>/dev/null)
        
        if [ "$tg_pkts" -gt 0 ] 2>/dev/null; then
            deliver_ratio=$(echo "scale=1; $total_rx * 100 / $tg_pkts" | bc 2>/dev/null)
            echo "  │ TG 发送:       ${tg_pps} pps (${tg_mbps} Mbps)"
            echo "  │ 防火墙处理:    ${fw_pps} pps (${fw_mbps} Mbps)"
            echo "  │ 交付率:        ${deliver_ratio}%"
            echo "  │ 丢包率:        $(echo "scale=1; 100 - $deliver_ratio" | bc 2>/dev/null)%"
        fi
    fi
else
    echo "  │ 未检测到防火墙收包，请检查："
    echo "  │ 1. ${TRAFFIC_NIC}/ens160/ens192 是否在同一 VMware 虚拟交换机"
    echo "  │ 2. 检查数据面日志: cat ${LOG_FILE}"
fi
echo "  └────────────────────────────────────────────────┘"

echo ""
echo "完整日志: ${LOG_FILE}"
