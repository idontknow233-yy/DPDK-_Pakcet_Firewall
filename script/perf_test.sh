#!/usr/bin/env bash
set -euo pipefail

DPI=/home/yy/dpdk/dpdk-stable-24.11.4/usertools/dpdk-devbind.py
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
FW_BIN="${ROOT_DIR}/build/dataplane/dpdk_packet_firewall"
LOG_DIR="${ROOT_DIR}/log"
FW_LOG="${LOG_DIR}/perf_fw.log"
TG_LOG="${LOG_DIR}/perf_tg.log"
DPDK_LIB=/home/yy/dpdk/dpdk-stable-24.11.4/dpdklib/lib/x86_64-linux-gnu
export LD_LIBRARY_PATH="${DPDK_LIB}:${LD_LIBRARY_PATH:-}"

PORT0_PCI="0000:03:00.0"  # ens160 - FW Port 0
PORT1_PCI="0000:0b:00.0"  # ens192 - FW Port 1
TRAFFIC_NIC="ens224"       # ens224 - 流量发送 (kernel driver)

PORT0_MAC="00:0c:29:fb:49:f3"
PORT1_MAC="00:0c:29:fb:49:fd"

RED='\033[0;31m'; GRN='\033[0;32m'; CYAN='\033[0;36m'; NC='\033[0m'
log()  { echo -e "${CYAN}[$(date +%T)]${NC} $*"; }
ok()   { echo -e "${GRN}[OK]${NC} $*"; }
err()  { echo -e "${RED}[ERR]${NC} $*"; }

cleanup() {
    log "清理..."
    for pid in $(cat /tmp/perf_fw.pid /tmp/perf_tg_pids 2>/dev/null); do
        kill -9 "$pid" 2>/dev/null || true
    done
    sleep 1
    for pci in "$PORT0_PCI" "$PORT1_PCI"; do
        python3 "$DPI" --noiommu-mode --bind=vmxnet3 "$pci" 2>/dev/null || true
    done
    ip link set ens160 up 2>/dev/null || true
    ip link set ens192 up 2>/dev/null || true
    ip link set ens224 up 2>/dev/null || true
    rm -f /tmp/perf_*.pid /tmp/perf_tg_pids /tmp/perf_tg_result
}
trap cleanup EXIT

# ==================== Step 1: 绑定 FW 网卡 ====================
log "Step 1: 绑定防火墙 VMXNET3 NIC 到 vfio-pci"

if [ "$(cat /sys/module/vfio/parameters/enable_unsafe_noiommu_mode 2>/dev/null)" != "Y" ]; then
    echo 1 | tee /sys/module/vfio/parameters/enable_unsafe_noiommu_mode >/dev/null
fi

for pci in "$PORT0_PCI" "$PORT1_PCI"; do
    ip link set "${TRAFFIC_NIC}" down 2>/dev/null || true
    python3 "$DPI" --noiommu-mode --bind=vmxnet3 "$pci" 2>/dev/null || true
    python3 "$DPI" --noiommu-mode --bind=vfio-pci "$pci"
done

python3 "$DPI" --status 2>/dev/null | grep -E "Network|03:00|0b:00"
python3 "$DPI" --status 2>/dev/null | grep -E "13:00"
ok "FW NIC 绑定完成"

# ==================== Step 2: 配置流量口 ====================
log "Step 2: 配置流量发送接口 ($TRAFFIC_NIC)"
ip link set "$TRAFFIC_NIC" up 2>/dev/null || true
ip addr flush dev "$TRAFFIC_NIC" 2>/dev/null || true
ok "${TRAFFIC_NIC} 已就绪"

# ==================== Step 3: 停旧进程 ====================
log "Step 3: 停止旧进程"
pgrep -f "[d]pdk_packet_firewall" | xargs -r kill -9 2>/dev/null || true
pgrep -f "[t]raffic_gen" | xargs -r kill -9 2>/dev/null || true
sleep 1
ok "旧进程已清理"

# ==================== Step 4: 启动防火墙 ====================
log "Step 4: 启动防火墙"
echo ""
echo "  [防火墙参数]  L2转发 / 无ACL / 混杂ON / lcore 0-1"
echo ""

"$FW_BIN" \
    -l 0-1 --proc-type=primary -- \
    -p 0x3 -P -T 5 >"${FW_LOG}" 2>&1 &
FW_PID=$!
echo "$FW_PID" > /tmp/perf_fw.pid

log "等待防火墙初始化..."
for i in $(seq 1 30); do
    if grep -q "entering main loop" "${FW_LOG}" 2>/dev/null; then
        ok "防火墙已启动 (PID=$FW_PID)"
        break
    fi
    if ! kill -0 "$FW_PID" 2>/dev/null; then
        err "防火墙启动失败:"; tail -30 "${FW_LOG}"; exit 1
    fi
    sleep 1
done
sleep 2

# ==================== Step 5: 生成流量脚本 ====================
log "Step 5: 生成流量脚本"
echo ""

cat > /tmp/traffic_gen.py << 'PYEOF'
import socket, time, struct, sys, os, threading

DST_MAC = sys.argv[1] if len(sys.argv) > 1 else "00:0c:29:fb:49:f3"
SRC_MAC = sys.argv[2] if len(sys.argv) > 2 else "00:0c:29:fb:49:07"
IFACE   = sys.argv[3] if len(sys.argv) > 3 else "ens224"
PKT_SZ  = int(os.environ.get("PKT_SIZE", "1400"))
DUR     = 30
NTHREADS = 8

def mac(s): return bytes(int(b,16) for b in s.split(':'))

dst_mac = mac(DST_MAC)
src_mac = mac(SRC_MAC)
payload_sz = PKT_SZ - 14 - 20 - 8

# IP header: version_ihl,tos,len,id,flags_frag,ttl,proto,cksum(0),src_ip,dst_ip
ih_temp = struct.pack('!BBHHHBBH', 0x45, 0, 20+8+payload_sz, 0, 0, 64, 17, 0)
ip_hdr = ih_temp + struct.pack('!II', 0x0a000001, 0x0a000002)
# compute checksum over full 20-byte IP header
cs = 0
for i in range(0, len(ip_hdr), 2):
    cs += (ip_hdr[i] << 8) + ip_hdr[i+1]
while cs >> 16:
    cs = (cs & 0xffff) + (cs >> 16)
cksum = ~cs & 0xffff
ip_hdr = ih_temp[:10] + struct.pack('!H', cksum) + ip_hdr[12:]
udp_hdr = struct.pack('!HHHH', 12345, 80, 8+payload_sz, 0)
frame = dst_mac + src_mac + struct.pack('!H',0x0800) + ip_hdr + udp_hdr + b'\x00'*payload_sz

total = [0]
lock = threading.Lock()
running = [True]
start_ts = [0.0]

def sender(tid):
    sock = socket.socket(socket.AF_PACKET, socket.SOCK_RAW)
    sock.bind((IFACE, 0))
    cnt = 0
    end = start_ts[0] + DUR
    while time.time() < end:
        for _ in range(32):
            try:
                sock.send(frame)
                cnt += 1
            except:
                break
        with lock:
            total[0] += cnt
            cnt = 0
    with lock:
        total[0] += cnt
    sock.close()

print(f"[TG] pkt_size={PKT_SZ}B threads={NTHREADS} duration={DUR}s dst={DST_MAC}", flush=True)

threads = [threading.Thread(target=sender, args=(i,)) for i in range(NTHREADS)]
start_ts[0] = time.time()
for t in threads:
    t.start()

last_cnt = 0
last_ts = time.time()
while time.time() - start_ts[0] < DUR:
    time.sleep(2)
    with lock:
        cur = total[0]
    elapsed = time.time() - start_ts[0]
    dt = time.time() - last_ts
    pps = (cur - last_cnt) / dt if dt > 0 else 0
    mbps = pps * PKT_SZ * 8 / 1e6
    print(f"[TG] {cur:>10d} pkts | {elapsed:>5.1f}s | {pps:>10.0f} pps | {mbps:>7.1f} Mbps", flush=True)
    last_cnt = cur
    last_ts = time.time()

running[0] = False
for t in threads:
    t.join()

with lock:
    all_total = total[0]
elapsed = time.time() - start_ts[0]
pps = all_total / elapsed if elapsed > 0 else 0
mbps = all_total * PKT_SZ * 8 / elapsed / 1e6 if elapsed > 0 else 0
print(f"\n[TG] DONE pkts={all_total} elapsed={elapsed:.1f}s pps={pps:.0f} mbps={mbps:.1f}", flush=True)

with open("/tmp/perf_tg_result", "w") as f:
    f.write(f"pkts={all_total}\nelapsed={elapsed:.2f}\npps={pps:.0f}\nmbps={mbps:.1f}\npkt_size={PKT_SZ}\n")
PYEOF

ok "流量脚本已生成"

# ==================== Step 6: 启动流量 ====================
FW_TG_MAC=$(cat /sys/class/net/${TRAFFIC_NIC}/address 2>/dev/null || echo "00:0c:29:fb:49:07")
log "Step 6: 启动流量生成器 (${TRAFFIC_NIC}, src=${FW_TG_MAC}, 8线程[BURST=256], 30秒)"

PKT_SIZE=64 python3 /tmp/traffic_gen.py "${PORT0_MAC}" "${FW_TG_MAC}" "${TRAFFIC_NIC}" >"${TG_LOG}" 2>&1 &
echo $! > /tmp/perf_tg_pids
ok "流量生成器已启动 (PID=$(cat /tmp/perf_tg_pids))"

# 监控 TG 进程
log "Step 7: 测试运行中... (后台监控每5秒输出)"
echo ""
echo "  ─── 实时统计 (每5秒) ───"
echo ""

prev_rx=0; prev_tx=0
TG_PID=$(cat /tmp/perf_tg_pids)

while kill -0 "$TG_PID" 2>/dev/null; do
    sleep 5
    total_rx=$(grep "Total packets received:" "${FW_LOG}" | tail -1 | grep -oP '\d+')
    total_tx=$(grep "Total packets sent:" "${FW_LOG}" | tail -1 | grep -oP '\d+')
    total_drop=$(grep "Total packets dropped:" "${FW_LOG}" | tail -1 | grep -oP '\d+')
    
    if [ -n "$total_rx" ] && [ -n "$total_tx" ]; then
        delta_rx=$((total_rx - prev_rx))
        delta_tx=$((total_tx - prev_tx))
        pps_rx=$((delta_rx / 5))
        mbps_rx=$(echo "scale=1; $delta_rx * 1400 * 8 / 5 / 1000000" | bc 2>/dev/null || echo "N/A")
        echo -e "  [$(date +%T)] FW: RX=${total_rx}(+${delta_rx}) ${pps_rx}pps ${mbps_rx}Mbps | TX=${total_tx}(+${delta_tx}) pps | DROP=$total_drop"
        prev_rx=$total_rx
        prev_tx=$total_tx
    fi
done

wait "$TG_PID" || true

# ==================== Step 8: 最终统计 ====================
tg_pkts=$(grep "^pkts=" /tmp/perf_tg_result 2>/dev/null | cut -d= -f2)
tg_elapsed=$(grep "^elapsed=" /tmp/perf_tg_result 2>/dev/null | cut -d= -f2)
tg_pps=$(grep "^pps=" /tmp/perf_tg_result 2>/dev/null | cut -d= -f2)
tg_mbps=$(grep "^mbps=" /tmp/perf_tg_result 2>/dev/null | cut -d= -f2)

total_rx=$(grep "Total packets received:" "${FW_LOG}" | tail -1 | grep -oP '\d+')
total_tx=$(grep "Total packets sent:" "${FW_LOG}" | tail -1 | grep -oP '\d+')
total_drop=$(grep "Total packets dropped:" "${FW_LOG}" | tail -1 | grep -oP '\d+')

log "Step 8: 最终统计"
echo ""
echo "  ┌─── 流量生成器 (${TRAFFIC_NIC}) ───┐"
echo "  │ 总发包数:    ${tg_pkts:-N/A}"
echo "  │ 发送速率:    ${tg_pps:-N/A} pps"
echo "  │ 发送带宽:    ${tg_mbps:-N/A} Mbps"
echo "  └──────────────────────────┘"
echo ""
echo "  ┌─── 防火墙 (DPDK) ────────┐"
echo "  │ 接收包数:    ${total_rx:-N/A}"
echo "  │ 发送包数:    ${total_tx:-N/A}"
echo "  │ 丢包数:      ${total_drop:-N/A}"
echo "  └──────────────────────────┘"

if [ -n "$total_rx" ] && [ "$total_rx" -gt 0 ] 2>/dev/null && [ -n "$tg_pkts" ] && [ "$tg_pkts" != "0" ] 2>/dev/null; then
    delivery=$(echo "scale=1; $total_rx * 100 / $tg_pkts" | bc 2>/dev/null)
    fw_pps=$(echo "scale=0; $total_rx / $tg_elapsed" | bc 2>/dev/null)
    fw_mbps=$(echo "scale=1; $total_rx * 1400 * 8 / $tg_elapsed / 1000000" | bc 2>/dev/null)
    echo ""
    echo "  ┌─── 结果分析 ──────────────────────────────┐"
    echo "  │ 防火墙吞吐:  ${fw_pps} pps (${fw_mbps} Mbps)"
    echo "  │ 交付率:      ${delivery}%"
    echo "  └────────────────────────────────────────────┘"
fi
