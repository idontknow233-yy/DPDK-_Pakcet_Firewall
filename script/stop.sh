#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
LOG_DIR="${ROOT_DIR}/log"
PID_DIR="${LOG_DIR}/pids"

mkdir -p "${LOG_DIR}" "${PID_DIR}"

timestamp() { date +"%F %T"; }

echo "[$(timestamp)] stop.sh: 强制清理所有相关进程..."

pkill -9 -f "go run ./cmd/server" || true
pkill -9 -f "npm run dev" || true
pkill -9 -f "control_plane" || true
pkill -9 -f "dpdk_packet_firewall" || true

sleep 2

for f in "${LOG_DIR}"/*.log; do
  [[ -e "$f" ]] || continue
  : >"$f" 2>/dev/null || echo -n >"$f" 2>/dev/null || true
done

for f in "${PID_DIR}"/*.pid; do
  [[ -e "$f" ]] || continue
  rm -f "$f"
done

STOP_LOG="${LOG_DIR}/stop.log"
touch "${STOP_LOG}"
exec > >(tee -a "${STOP_LOG}") 2>&1

echo "[$(timestamp)] stop.sh: ROOT_DIR=${ROOT_DIR}"

kill_processes_on_ports() {
  local ports=("8086" "9000")
  for port in "${ports[@]}"; do
    local pids
    pids="$(lsof -ti:${port} 2>/dev/null || true)"
    if [[ -n "${pids}" ]]; then
      echo "[$(timestamp)] 停止占用端口 ${port} 的进程: ${pids}"
      kill -9 ${pids} 2>/dev/null || true
    fi
  done
}

stop_pid() {
  local name="$1"
  local pidfile="${PID_DIR}/${name}.pid"

  if [[ ! -f "${pidfile}" ]]; then
    echo "[$(timestamp)] [${name}] pid 文件不存在，跳过 (${pidfile})"
    return 0
  fi

  local pid
  pid="$(cat "${pidfile}" 2>/dev/null || true)"
  if [[ -z "${pid}" ]]; then
    echo "[$(timestamp)] [${name}] pid 文件为空，删除 pid 文件"
    rm -f "${pidfile}"
    return 0
  fi

  if ! kill -0 "${pid}" 2>/dev/null; then
    echo "[$(timestamp)] [${name}] 进程不存在 (pid=${pid})，删除 pid 文件"
    rm -f "${pidfile}"
    return 0
  fi

  echo "[$(timestamp)] [${name}] 停止 (pid=${pid})"
  kill -TERM "${pid}" 2>/dev/null || true

  local i
  for i in {1..30}; do
    if kill -0 "${pid}" 2>/dev/null; then
      sleep 0.2
    else
      echo "[$(timestamp)] [${name}] 已停止"
      rm -f "${pidfile}"
      return 0
    fi
  done

  echo "[$(timestamp)] [${name}] 超时未退出，强制 kill -KILL (pid=${pid})"
  kill -KILL "${pid}" 2>/dev/null || true
  rm -f "${pidfile}"
}

stop_fallback() {
  local label="$1"
  local pattern="$2"
  local pids
  pids="$(pgrep -f "${pattern}" 2>/dev/null || true)"
  if [[ -z "${pids}" ]]; then
    echo "[$(timestamp)] [${label}] 未找到匹配进程，跳过"
    return 0
  fi
  echo "[$(timestamp)] [${label}] fallback 停止: ${pids}"
  pkill -TERM -f "${pattern}" 2>/dev/null || true
}

kill_processes_on_ports

stop_pid "frontend"
stop_pid "backend"
stop_pid "controlplane"
stop_pid "dataplane"

stop_fallback "frontend" "/home/yy/DPDK_Packet_Firewall/web/frontend.*(npm|vite)"
stop_fallback "backend" "/home/yy/DPDK_Packet_Firewall/web/backend.*go run ./cmd/server"
stop_fallback "controlplane" "/home/yy/DPDK_Packet_Firewall/build/controlplane/control_plane"
stop_fallback "dataplane" "/home/yy/DPDK_Packet_Firewall/build/dataplane/dpdk_packet_firewall"

echo "[$(timestamp)] stop.sh: 完成"
