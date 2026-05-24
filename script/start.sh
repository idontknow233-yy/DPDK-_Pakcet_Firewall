#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
LOG_DIR="${ROOT_DIR}/log"
PID_DIR="${LOG_DIR}/pids"

mkdir -p "${LOG_DIR}" "${PID_DIR}"

export LD_LIBRARY_PATH="/home/yy/dpdk/dpdk-stable-24.11.4/dpdkbuild/lib:${LD_LIBRARY_PATH:-}"
export PATH="/usr/local/go/bin:${PATH}"

for f in "${LOG_DIR}"/*.log; do
  [[ -e "$f" ]] || continue
  if ! : >"$f" 2>/dev/null; then
    echo "[$(timestamp)] 警告: 无法清空日志文件 $f，尝试使用 tr..."
    if command -v truncate >/dev/null 2>&1; then
      truncate -s 0 "$f" 2>/dev/null || true
    else
      echo -n "" >"$f" 2>/dev/null || true
    fi
  fi
done

START_LOG="${LOG_DIR}/start.log"
touch "${START_LOG}"
exec > >(tee -a "${START_LOG}") 2>&1

timestamp() { date +"%F %T"; }

run_step() {
  local name="$1"
  shift
  local logfile="${LOG_DIR}/${name}.log"
  echo "[$(timestamp)] [${name}] 开始"
  echo "[$(timestamp)] [${name}] CMD: $*"
  "$@" >>"${logfile}" 2>&1
  echo "[$(timestamp)] [${name}] 完成 (exit=$?)"
}

start_bg() {
  local name="$1"
  local logfile="${LOG_DIR}/${name}.log"
  local pidfile="${PID_DIR}/${name}.pid"
  shift

  if [[ -f "${pidfile}" ]]; then
    local old_pid
    old_pid="$(cat "${pidfile}" 2>/dev/null || true)"
    if [[ -n "${old_pid}" ]] && kill -0 "${old_pid}" 2>/dev/null; then
      echo "[$(timestamp)] [${name}] 已在运行 (pid=${old_pid})"
      return 0
    fi
    rm -f "${pidfile}"
  fi

  echo "[$(timestamp)] [${name}] 启动"
  echo "[$(timestamp)] [${name}] CMD: $*"
  stdbuf -oL -eL nohup "$@" >>"${logfile}" 2>&1 &
  local pid=$!
  echo "${pid}" >"${pidfile}"

  local i
  for i in {1..20}; do
    if kill -0 "${pid}" 2>/dev/null; then
      sleep 0.2
    else
      echo "[$(timestamp)] [${name}] 启动失败，进程已退出 (pid=${pid})"
      tail -n 80 "${logfile}" || true
      return 1
    fi
  done

  echo "[$(timestamp)] [${name}] 已启动 (pid=${pid})"
}

wait_tcp() {
  local host="$1"
  local port="$2"
  local name="$3"
  local i
  for i in {1..60}; do
    if timeout 0.5 bash -c ">/dev/tcp/${host}/${port}" 2>/dev/null; then
      echo "[$(timestamp)] [${name}] 端口已就绪 ${host}:${port}"
      return 0
    fi
    sleep 0.5
  done
  echo "[$(timestamp)] [${name}] 等待端口超时 ${host}:${port}"
  return 1
}

echo "[$(timestamp)] start.sh: ROOT_DIR=${ROOT_DIR}"
echo "[$(timestamp)] start.sh: LOG_DIR=${LOG_DIR}"

run_step "devbind" /home/yy/dpdk/dpdk-stable-24.11.4/usertools/dpdk-devbind.py --noiommu-mode -b vfio-pci 0000:02:08.0

start_bg "dataplane" /home/yy/DPDK_Packet_Firewall/build/dataplane/dpdk_packet_firewall -l 0-3 -n 4 --proc-type=primary -- -p 0x1 -P

echo "[$(timestamp)] [dataplane] 等待 dataplane 完全初始化..."
for i in {1..50}; do
  if grep -q "L2FWD: entering main loop" "${LOG_DIR}/dataplane.log" 2>/dev/null; then
    echo "[$(timestamp)] [dataplane] 初始化完成"
    break
  fi
  if [[ $i -eq 50 ]]; then
    echo "[$(timestamp)] [dataplane] 初始化超时"
    tail -n 50 "${LOG_DIR}/dataplane.log"
    exit 1
  fi
  sleep 0.5
done

sleep 2

start_bg "controlplane" /home/yy/DPDK_Packet_Firewall/build/controlplane/control_plane -l 0-3 -n 4 --proc-type=secondary -- --cli-host 0.0.0.0 --cli-port 8086

echo "[$(timestamp)] [controlplane] 等待 controlplane 启动..."
for i in {1..60}; do
  pid="$(cat "${PID_DIR}/controlplane.pid" 2>/dev/null || echo "")"
  if [[ -z "${pid}" ]] || ! kill -0 "${pid}" 2>/dev/null; then
    echo "[$(timestamp)] [controlplane] 进程已退出 (pid=${pid})"
    echo "[$(timestamp)] [controlplane] ========== 完整日志输出 =========="
    cat "${LOG_DIR}/controlplane.log" || true
    echo "[$(timestamp)] [controlplane] ========== 日志结束 =========="
    exit 1
  fi
  
  if grep -q "ACL CLI listening on 0.0.0.0:8086" "${LOG_DIR}/controlplane.log" 2>/dev/null; then
    echo "[$(timestamp)] [controlplane] 启动成功"
    sleep 1
    if ! kill -0 "${pid}" 2>/dev/null; then
      echo "[$(timestamp)] [controlplane] 启动后立即崩溃 (pid=${pid})"
      echo "[$(timestamp)] [controlplane] ========== 完整日志输出 =========="
      cat "${LOG_DIR}/controlplane.log" || true
      echo "[$(timestamp)] [controlplane] ========== 日志结束 =========="
      exit 1
    fi
    break
  fi
  
  if [[ $((i % 10)) -eq 0 ]]; then
    echo "[$(timestamp)] [controlplane] 等待中... (${i}/60)"
  fi
  
  if [[ $i -eq 60 ]]; then
    echo "[$(timestamp)] [controlplane] 启动超时"
    echo "[$(timestamp)] [controlplane] ========== 日志最后100行 =========="
    tail -n 100 "${LOG_DIR}/controlplane.log" || true
    echo "[$(timestamp)] [controlplane] ========== 日志结束 =========="
    exit 1
  fi
  sleep 0.5
done

# wait_tcp "127.0.0.1" "8086" "controlplane"

(
  cd /home/yy/DPDK_Packet_Firewall/web/backend
  start_bg "backend" env CLI_HOST=127.0.0.1 CLI_PORT=8086 HTTP_ADDR=:9000 go run ./cmd/server
)
wait_tcp "127.0.0.1" "9000" "backend"

(
  cd /home/yy/DPDK_Packet_Firewall/web/frontend
  start_bg "frontend" npm run dev
)

echo "[$(timestamp)] start.sh: 全部启动完成"
