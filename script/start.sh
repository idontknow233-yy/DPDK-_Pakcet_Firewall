#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
LOG_DIR="${ROOT_DIR}/log"
PID_DIR="${LOG_DIR}/pids"

mkdir -p "${LOG_DIR}" "${PID_DIR}"

START_LOG="${LOG_DIR}/start.log"
touch "${START_LOG}"
exec > >(tee -a "${START_LOG}") 2>&1

timestamp() { date +"%F %T"; }

have_cmd() { command -v "$1" >/dev/null 2>&1; }

STDBUF_PREFIX=()
if have_cmd stdbuf; then
  STDBUF_PREFIX=(stdbuf -oL -eL)
fi

find_pid() {
  local pattern="$1"
  pgrep -f "${pattern}" 2>/dev/null | head -n 1 || true
}

port_open() {
  local host="$1"
  local port="$2"
  timeout 0.3 bash -c ">/dev/tcp/${host}/${port}" >/dev/null 2>&1
}

wait_log_contains() {
  local name="$1"
  local logfile="$2"
  local needle="$3"
  local i
  for i in {1..60}; do
    if grep -Fq "${needle}" "${logfile}" 2>/dev/null; then
      echo "[$(timestamp)] [${name}] 日志就绪: ${needle}"
      return 0
    fi
    sleep 0.5
  done
  echo "[$(timestamp)] [${name}] 等待日志超时: ${needle}"
  tail -n 120 "${logfile}" || true
  return 1
}

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
  local pattern=""
  if [[ "${1:-}" == "--pattern" ]]; then
    pattern="${2:-}"
    shift 2
  fi
  if [[ -z "${pattern}" ]]; then
    pattern="${1:-}"
  fi

  if [[ -f "${pidfile}" ]]; then
    local old_pid
    old_pid="$(cat "${pidfile}" 2>/dev/null || true)"
    if [[ -n "${old_pid}" ]] && kill -0 "${old_pid}" 2>/dev/null; then
      echo "[$(timestamp)] [${name}] 已在运行 (pid=${old_pid})"
      return 0
    fi
    rm -f "${pidfile}"
  fi

  local existing_pid
  existing_pid="$(find_pid "${pattern}")"
  if [[ -n "${existing_pid}" ]] && kill -0 "${existing_pid}" 2>/dev/null; then
    echo "[$(timestamp)] [${name}] 检测到已在运行 (pid=${existing_pid})"
    echo "${existing_pid}" >"${pidfile}"
    return 0
  fi

  echo "[$(timestamp)] [${name}] 启动"
  echo "[$(timestamp)] [${name}] CMD: $*"
  nohup "${STDBUF_PREFIX[@]}" "$@" >>"${logfile}" 2>&1 &
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

run_step "devbind" /home/yy/dpdk/dpdk-stable-24.11.4/usertools/dpdk-devbind.py --noiommu-mode -b vfio-pci 0000:02:05.0
run_step "devbind" /home/yy/dpdk/dpdk-stable-24.11.4/usertools/dpdk-devbind.py --noiommu-mode -b vfio-pci 0000:02:06.0
run_step "devbind" /home/yy/dpdk/dpdk-stable-24.11.4/usertools/dpdk-devbind.py --noiommu-mode -b vfio-pci 0000:02:07.0
run_step "devbind" /home/yy/dpdk/dpdk-stable-24.11.4/usertools/dpdk-devbind.py --noiommu-mode -b vfio-pci 0000:02:08.0

start_bg "dataplane" --pattern "/home/yy/DPDK_Packet_Firewall/build/dataplane/dpdk_packet_firewall" /home/yy/DPDK_Packet_Firewall/build/dataplane/dpdk_packet_firewall -l 0-3 -n 4 --proc-type=primary -- -p 0x3 -P

if port_open "127.0.0.1" "8086"; then
  echo "[$(timestamp)] [controlplane] 端口已被占用 127.0.0.1:8086"
fi
start_bg "controlplane" --pattern "/home/yy/DPDK_Packet_Firewall/build/controlplane/control_plane" /home/yy/DPDK_Packet_Firewall/build/controlplane/control_plane -l 0-3 -n 4 --proc-type=secondary -- --cli-host 0.0.0.0 --cli-port 8086
wait_tcp "127.0.0.1" "8086" "controlplane"
wait_log_contains "controlplane" "${LOG_DIR}/controlplane.log" "ACL CLI listening"

(
  cd /home/yy/DPDK_Packet_Firewall/web/backend
  if port_open "127.0.0.1" "9000"; then
    echo "[$(timestamp)] [backend] 端口已被占用 127.0.0.1:9000"
  fi
  start_bg "backend" --pattern "go run ./cmd/server" env CLI_HOST=127.0.0.1 CLI_PORT=8086 HTTP_ADDR=:9000 go run ./cmd/server
)
wait_tcp "127.0.0.1" "9000" "backend"
wait_log_contains "backend" "${LOG_DIR}/backend.log" "listening :9000"

(
  cd /home/yy/DPDK_Packet_Firewall/web/frontend
  start_bg "frontend" --pattern "DPDK_Packet_Firewall/web/frontend.*npm run dev" npm run dev
)
wait_tcp "127.0.0.1" "5173" "frontend"

echo "[$(timestamp)] start.sh: 全部启动完成"
