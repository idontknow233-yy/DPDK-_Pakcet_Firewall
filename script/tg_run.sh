#!/usr/bin/env bash
set -euo pipefail

# ===== 发包 VM 运行脚本 =====
# 用法: sudo bash tg_run.sh
# 前提: DPDK 24.11 库已安装, hugepages 已配置

TG="./tg_dpdk"
# 改为发包 VM 上 dpdklib 解压后的路径
DPDK_LIB="${HOME}/dpdklib/lib/x86_64-linux-gnu"
export LD_LIBRARY_PATH="${DPDK_LIB}:${LD_LIBRARY_PATH:-}"

IFACE="ens192"                # 发包网卡名 (VMXNET3 on VMnet2)
DST_MAC="00:0c:29:fb:49:f3"  # FW Port 0 MAC
DUR=30
SIZES=(64 128 256 512 1024 1514)

echo "PKTGEN VM: 吞吐量发包测试"
echo "  接口: ${IFACE}"
echo "  目标: ${DST_MAC}"
echo "  时长: ${DUR}s/轮"
echo ""

for sz in "${SIZES[@]}"; do
    echo "=== ${sz}B ==="
    "${TG}" --no-pci --file-prefix tg \
        --vdev="net_af_packet0,iface=${IFACE}" \
        -l 0 --proc-type=primary -- \
        "${sz}" "${DUR}" "${DST_MAC}"
    echo ""
done

echo "全部完成"
