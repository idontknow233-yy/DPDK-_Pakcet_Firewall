#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
FW_BIN="${ROOT_DIR}/build/dataplane/dpdk_packet_firewall"
DPI=/home/yy/dpdk/dpdk-stable-24.11.4/usertools/dpdk-devbind.py
DPDK_LIB=/home/yy/dpdk/dpdk-stable-24.11.4/dpdklib/lib/x86_64-linux-gnu
export LD_LIBRARY_PATH="${DPDK_LIB}:${LD_LIBRARY_PATH:-}"

PORT0_PCI="0000:03:00.0"; PORT1_PCI="0000:0b:00.0"
TRAFFIC_NIC="ens224"; PORT0_MAC="00:0c:29:fb:49:f3"; TG_MAC="00:0c:29:fb:49:07"
DUR=30
SIZES=(64 128 256 512 1024 1518)

GRN='\033[0;32m'; CYAN='\033[0;36m'; NC='\033[0m'
log() { echo -e "${CYAN}[$(date +%T)]${NC} $*"; }

cleanup() {
    kill -9 $(cat /tmp/tp_fw.pid 2>/dev/null) 2>/dev/null || true; sleep 1
    for pci in "$PORT0_PCI" "$PORT1_PCI"; do
        python3 "$DPI" --noiommu-mode --bind=vmxnet3 "$pci" 2>/dev/null || true
    done
    ip link set ens160 up 2>/dev/null; ip link set ens192 up 2>/dev/null
    rm -f /tmp/tp_fw.pid /tmp/tp_fw.log /tmp/tp_tg.log
}
trap cleanup EXIT

# ------------ traffic gen ------------
cat > /tmp/tg.py << 'PYEOF'
import socket,time,struct,os,threading
DST_MAC=os.environ["TG_DSTMAC"]; SRC_MAC=os.environ["TG_SRCMAC"]
IFACE=os.environ["TG_IFACE"]; PKT_SZ=int(os.environ["TG_PKTSZ"]); DUR=int(os.environ["TG_DUR"])
def mac(s): return bytes(int(b,16)for b in s.split(":"))
dst_mac=mac(DST_MAC); src_mac=mac(SRC_MAC)
psz=max(PKT_SZ-14-20-8,0)
ih=struct.pack("!BBHHHBBH",0x45,0,20+8+psz,0,0,64,17,0)
ip=ih+struct.pack("!II",0x0a000001,0x0a000002)
cs=0
for i in range(0,len(ip),2):cs+=(ip[i]<<8)+ip[i+1]
while cs>>16:cs=(cs&0xffff)+(cs>>16)
cs=~cs&0xffff
ip=ih[:10]+struct.pack("!H",cs)+ip[12:]
udp=struct.pack("!HHHH",12345,80,8+psz,0)
frame=dst_mac+src_mac+struct.pack("!H",0x0800)+ip+udp+b"\x00"*psz
total=[0];lock=threading.Lock();start_ts=[0.0]
def sender(tid):
    sock=socket.socket(socket.AF_PACKET,socket.SOCK_RAW); sock.bind((IFACE,0))
    cnt=0;end=start_ts[0]+DUR
    while time.time()<end:
        for _ in range(32):
            try:sock.send(frame);cnt+=1
            except:break
        with lock:total[0]+=cnt;cnt=0
    with lock:total[0]+=cnt; sock.close()
threads=[threading.Thread(target=sender,args=(i,))for i in range(8)]
start_ts[0]=time.time()
for t in threads:t.start()
last_cnt=0;last_ts=time.time()
while time.time()-start_ts[0]<DUR:
    time.sleep(2); elapsed=time.time()-start_ts[0]
    with lock:cur=total[0]
    dt=time.time()-last_ts
    pps=(cur-last_cnt)/dt if dt>0 else 0
    print(f"TG_LIVE:{cur}:{elapsed:.1f}:{pps:.0f}",flush=True)
    last_cnt=cur;last_ts=time.time()
for t in threads:t.join()
all_total=total[0]; elapsed=time.time()-start_ts[0]
pps=all_total/elapsed if elapsed>0 else 0
mbps=pps*PKT_SZ*8/1e6
print(f"TG_DONE:{all_total}:{elapsed:.1f}:{pps:.0f}:{mbps:.1f}",flush=True)
PYEOF

run_tg() {
    local sz=$1
    local rx0 tx0 drop0
    rx0=$(grep -a "Total packets received:" /tmp/tp_fw.log 2>/dev/null|tail -1|grep -oP '\d+')
    tx0=$(grep -a "Total packets sent:"   /tmp/tp_fw.log 2>/dev/null|tail -1|grep -oP '\d+')
    drop0=$(grep -a "Total packets dropped:" /tmp/tp_fw.log 2>/dev/null|tail -1|grep -oP '\d+')
    rx0=${rx0:-0}; tx0=${tx0:-0}; drop0=${drop0:-0}

    TG_DSTMAC="${PORT0_MAC}" TG_SRCMAC="${TG_MAC}" TG_IFACE="${TRAFFIC_NIC}" \
        TG_PKTSZ="$sz" TG_DUR="$DUR" python3 /tmp/tg.py >/tmp/tp_tg.log 2>&1 &
    wait $! 2>/dev/null || true; sleep 3

    FW_RX=$(grep -a "Total packets received:" /tmp/tp_fw.log 2>/dev/null|tail -1|grep -oP '\d+')
    FW_TX=$(grep -a "Total packets sent:"   /tmp/tp_fw.log 2>/dev/null|tail -1|grep -oP '\d+')
    FW_DROP=$(grep -a "Total packets dropped:" /tmp/tp_fw.log 2>/dev/null|tail -1|grep -oP '\d+')
    FW_RX=${FW_RX:-0}; FW_TX=${FW_TX:-0}; FW_DROP=${FW_DROP:-0}
    FW_RX=$((FW_RX - rx0)); FW_TX=$((FW_TX - tx0)); FW_DROP=$((FW_DROP - drop0))
}

# ------------ setup ------------
log "绑定 NIC..."
[ "$(cat /sys/module/vfio/parameters/enable_unsafe_noiommu_mode 2>/dev/null)" != "Y" ] && echo 1 > /sys/module/vfio/parameters/enable_unsafe_noiommu_mode
for pci in "$PORT0_PCI" "$PORT1_PCI"; do
    python3 "$DPI" --noiommu-mode --bind=vmxnet3 "$pci" 2>/dev/null || true
    python3 "$DPI" --noiommu-mode --bind=vfio-pci "$pci"
done
pgrep -f "[d]pdk_packet_firewall" | xargs -r kill -9 2>/dev/null || true; sleep 1

log "启动防火墙..."
"$FW_BIN" -l 0-1 -- -p 0x3 -P -T 30 >/tmp/tp_fw.log 2>&1 &
echo $! >/tmp/tp_fw.pid
for i in $(seq 1 30); do grep -a -q "entering main loop" /tmp/tp_fw.log 2>/dev/null && break; sleep 1; done
sleep 2
echo -e "${GRN}[OK]${NC} FW已启动"
echo ""

# ------------ test ------------
echo "================================================"
echo "  吞吐量测试 (无ACL = 全通, ${DUR}s/轮)"
echo "================================================"
echo ""

for sz in "${SIZES[@]}"; do
    theory=$(echo "scale=3; 125 / (${sz} + 20)" | bc 2>/dev/null)
    log "测试 ${sz}B... (理论线速: ${theory} Mpps)"
    run_tg "$sz"

    tg_data=$(grep -a "TG_DONE" /tmp/tp_tg.log 2>/dev/null)
    tg_pkgs=$(echo "$tg_data" | cut -d: -f2)
    tg_elapsed=$(echo "$tg_data" | cut -d: -f3)
    tg_pps=$(echo "$tg_data" | cut -d: -f4)
    tg_mbps=$(echo "$tg_data" | cut -d: -f5)

    fw_mpps="N/A"; fw_mbps="N/A"
    if [ -n "${FW_RX}" ] && [ "${FW_RX}" -gt 0 ] 2>/dev/null && [ -n "$tg_elapsed" ] && [ "$tg_elapsed" != "0" ]; then
        fw_mpps=$(echo "scale=3; ${FW_RX} / ${tg_elapsed} / 1000000" | bc 2>/dev/null)
        fw_mbps=$(echo "scale=1; ${FW_RX} * ${sz} * 8 / ${tg_elapsed} / 1000000" | bc 2>/dev/null)
    fi

    echo "  TG: ${tg_pkgs:-?}pkts ${tg_pps:-?}pps ${tg_mbps:-?}Mbps"
    echo "  FW: rx=${FW_RX:-?} tx=${FW_TX:-?} drop=${FW_DROP:-?}  →  ${fw_mpps} Mpps / ${fw_mbps} Mbps"
    echo ""

    echo "${sz} ${tg_pkgs:-0} ${tg_pps:-0} ${tg_mbps:-0} ${FW_RX:-0} ${FW_TX:-0} ${FW_DROP:-0} ${fw_mpps} ${fw_mbps}" >> /tmp/tp_all.txt
    sleep 3
done

# ------------ summary ------------
echo ""
echo "================================================"
echo "  结果汇总"
echo "================================================"
echo ""
printf "%-8s %-10s %-12s %-12s %-10s %-10s\n" "包大小" "理论Mpps" "吞吐Mpps" "吞吐Mbps" "线速占比" "丢包"
echo "-----------------------------------------------------------------"
while read -r sz tg_pkgs tg_pps tg_mbps fw_rx fw_tx fw_drop fw_mpps fw_mbps; do
    theory=$(echo "scale=3; 125 / (${sz} + 20)" | bc 2>/dev/null)
    ratio="N/A"
    if [ "$fw_mpps" != "N/A" ] && [ -n "$theory" ]; then
        ratio=$(echo "scale=1; ${fw_mpps} / ${theory} * 100" | bc 2>/dev/null || echo "N/A")
    fi
    printf "%-8s %-10s %-12s %-12s %-10s%% %-10s\n" "${sz}B" "$theory" "$fw_mpps" "$fw_mbps" "$ratio" "$fw_drop"
done < /tmp/tp_all.txt

echo ""
log "测试完成"
