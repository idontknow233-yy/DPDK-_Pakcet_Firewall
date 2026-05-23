#!/usr/bin/env bash

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
FW_BIN="${ROOT_DIR}/build/dataplane/dpdk_packet_firewall"
DPI=/home/yy/dpdk/dpdk-stable-24.11.4/usertools/dpdk-devbind.py
DPDK_LIB=/home/yy/dpdk/dpdk-stable-24.11.4/dpdklib/lib/x86_64-linux-gnu
export LD_LIBRARY_PATH="${DPDK_LIB}:${LD_LIBRARY_PATH:-}"

PORT0_PCI="0000:03:00.0"; PORT1_PCI="0000:0b:00.0"
DUR=30
SIZES=(64 128 256 512 1024 1514)

GRN='\033[0;32m'; CYAN='\033[0;36m'; NC='\033[0m'
log()  { echo -e "${CYAN}[$(date +%T)]${NC} $*"; }
ok()   { echo -e "${GRN}[OK]${NC} $*"; }

cleanup() {
    kill -9 $(cat /tmp/tp_fw.pid 2>/dev/null) 2>/dev/null || true; sleep 1
    for pci in "$PORT0_PCI" "$PORT1_PCI"; do
        python3 "$DPI" --noiommu-mode --bind=vmxnet3 "$pci" 2>/dev/null || true
    done
    ip link set ens160 up 2>/dev/null; ip link set ens192 up 2>/dev/null
    rm -f /tmp/tp_fw.pid /tmp/tp_fw.log /tmp/tp_all.txt
}
trap cleanup EXIT

# ==================== Setup ====================
log "绑定 ens160+ens192 到 vfio-pci..."
[ "$(cat /sys/module/vfio/parameters/enable_unsafe_noiommu_mode 2>/dev/null)" != "Y" ] && echo 1 > /sys/module/vfio/parameters/enable_unsafe_noiommu_mode
for pci in "$PORT0_PCI" "$PORT1_PCI"; do
    python3 "$DPI" --noiommu-mode --bind=vmxnet3 "$pci" 2>/dev/null || true
    python3 "$DPI" --noiommu-mode --bind=vfio-pci "$pci"
done
pgrep -f "[d]pdk_packet_firewall" | xargs -r kill -9 2>/dev/null || true
sleep 1

log "启动防火墙..."
"$FW_BIN" -l 0-1 -- -p 0x3 -P -T 5 >/tmp/tp_fw.log 2>&1 &
echo $! > /tmp/tp_fw.pid
for i in $(seq 1 30); do grep -a -q "entering main loop" /tmp/tp_fw.log 2>/dev/null && break; sleep 1; done
sleep 2
ok "FW 已启动"
echo ""

# ==================== Test ====================
echo "============================================"
echo "  FW 吞吐量记录 (请在发包VM按序运行 tg_run.sh)"
echo "============================================"
echo ""

> /tmp/tp_all.txt

for sz in "${SIZES[@]}"; do
    theory=$(echo "scale=3; 125 / (${sz} + 20)" | bc 2>/dev/null)
    echo ""
    echo "================================================"
    echo "  >>> 请在发包 VM 执行: ${sz}B 包测试 <<<"
    echo "================================================"

    # 记录基线
    rx0=$(grep -a "Total packets received:" /tmp/tp_fw.log 2>/dev/null|tail -1|grep -oP '\d+')
    tx0=$(grep -a "Total packets sent:"   /tmp/tp_fw.log 2>/dev/null|tail -1|grep -oP '\d+')
    dr0=$(grep -a "Total packets dropped:" /tmp/tp_fw.log 2>/dev/null|tail -1|grep -oP '\d+')
    rx0=${rx0:-0}; tx0=${tx0:-0}; dr0=${dr0:-0}

    # 等发包完成 (手动在发包VM执行)
    read -p "  按 Enter 确认 ${sz}B 发包已结束..." dummy

    # 读增量
    fw_rx=$(grep -a "Total packets received:" /tmp/tp_fw.log 2>/dev/null|tail -1|grep -oP '\d+')
    fw_tx=$(grep -a "Total packets sent:"   /tmp/tp_fw.log 2>/dev/null|tail -1|grep -oP '\d+')
    fw_drop=$(grep -a "Total packets dropped:" /tmp/tp_fw.log 2>/dev/null|tail -1|grep -oP '\d+')
    fw_rx=${fw_rx:-0}; fw_tx=${fw_tx:-0}; fw_drop=${fw_drop:-0}
    delta_rx=$((fw_rx - rx0)); delta_tx=$((fw_tx - tx0)); delta_drop=$((fw_drop - dr0))

    # Port 0 单独统计
    p0_rx=$(grep -a -A4 "Statistics for port 0" /tmp/tp_fw.log | tail -1 | grep "Packets received:" | grep -oP '\d+' | tail -1)
    p0_tx=$(grep -a -A4 "Statistics for port 0" /tmp/tp_fw.log | tail -1 | grep "Packets sent:" | grep -oP '\d+' | tail -1)

    # Port 1 统计
    p1_tx=$(grep -a -A4 "Statistics for port 1" /tmp/tp_fw.log | tail -1 | grep "Packets sent:" | grep -oP '\d+' | tail -1)

    mpps="0.000"; mbps="0.0"; ratio="0.0"
    [ "$delta_rx" -gt 0 ] 2>/dev/null && {
        mpps=$(echo "scale=3; ${delta_rx} / ${DUR} / 1000000" | bc 2>/dev/null)
        mbps=$(echo "scale=1; ${delta_rx} * ${sz} * 8 / ${DUR} / 1000000" | bc 2>/dev/null)
        ratio=$(echo "scale=1; ${mpps} / ${theory} * 100" | bc 2>/dev/null || echo "0.0")
    }

    echo ""
    echo "  ┌─ ${sz}B 结果 ─────────────────────────┐"
    echo "  │ FW 增量: rx=${delta_rx} tx=${delta_tx} drop=${delta_drop}"
    echo "  │ Port 0:  rx=${p0_rx:-?} tx=${p0_tx:-?}"
    echo "  │ Port 1:  tx=${p1_tx:-?}"
    echo "  │ 吞吐:    ${mpps} Mpps / ${mbps} Mbps / ${ratio}% 线速"
    echo "  └────────────────────────────────────────┘"
    echo ""

    echo "${sz} ${delta_rx} ${delta_tx} ${delta_drop} ${mpps} ${mbps} ${ratio}" >> /tmp/tp_all.txt
done

# ==================== Summary ====================
echo ""
echo "============================================"
echo "  吞吐量测试结果"
echo "============================================"
printf "%-8s %-10s %-12s %-12s %-8s %-6s\n" "包大小" "理论Mpps" "吞吐Mpps" "吞吐Mbps" "线速%" "丢包"
echo "----------------------------------------------------------"
while read -r sz delta_rx delta_tx delta_drop mpps mbps ratio; do
    theory=$(echo "scale=3; 125 / (${sz} + 20)" | bc 2>/dev/null)
    printf "%-8s %-10s %-12s %-12s %-7s%% %-6s\n" "${sz}B" "$theory" "$mpps" "$mbps" "$ratio" "$delta_drop"
done < /tmp/tp_all.txt
echo ""
log "测试完成"
