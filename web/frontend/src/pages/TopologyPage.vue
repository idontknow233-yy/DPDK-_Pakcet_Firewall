<template>
  <div>
    <el-card>
      <div class="topo-head">
        <div class="topo-title">实时拓扑与链路状态</div>
        <div class="topo-actions">
          <el-select v-model="refreshMs" style="width: 160px">
            <el-option :value="1000" label="1秒刷新" />
            <el-option :value="2000" label="2秒刷新" />
            <el-option :value="5000" label="5秒刷新" />
          </el-select>
          <el-button :loading="loading" @click="refresh">刷新</el-button>
        </div>
      </div>
      <div class="topo-meta">版本 {{ snap?.version ?? 0 }}，最近更新 {{ lastUpdated || '-' }}</div>

      <div class="topo-canvas">
        <div class="fw-box">
          <div class="fw-name">DPDK Packet Firewall</div>
          <div class="fw-sub">Ports: {{ ports.length }}</div>
        </div>
        <div class="port-grid">
          <div v-for="p in ports" :key="p.port" class="port-card" :class="p.link === 'up' ? 'up' : 'down'">
            <div class="port-head">
              <div class="port-id">Port {{ p.port }}</div>
              <el-tag size="small" :type="p.link === 'up' ? 'success' : 'danger'" effect="light">
                {{ p.link === 'up' ? 'UP' : 'DOWN' }}
              </el-tag>
            </div>
            <div class="port-line">
              <span class="k">MAC</span>
              <span class="v">{{ p.mac || '-' }}</span>
            </div>
            <div class="port-line">
              <span class="k">Speed</span>
              <span class="v">{{ p.speed ? `${p.speed}Mbps` : '-' }}</span>
            </div>
            <div class="port-line">
              <span class="k">Duplex</span>
              <span class="v">{{ p.duplex || '-' }}</span>
            </div>
            <div class="port-line">
              <span class="k">RX PPS</span>
              <span class="v">{{ p.rx_pps }}</span>
            </div>
            <div class="port-line">
              <span class="k">TX PPS</span>
              <span class="v">{{ p.tx_pps }}</span>
            </div>
            <div class="port-line">
              <span class="k">DROP PPS</span>
              <span class="v">{{ p.drop_pps }}</span>
            </div>
          </div>
        </div>
      </div>
    </el-card>
  </div>
</template>

<script lang="ts" setup>
import { computed, onMounted, onUnmounted, ref, watch } from 'vue'
import { ElMessage } from 'element-plus'
import { getPortStats, type PortStatsRow, type PortStatsSnapshot } from '../services/api'

const loading = ref(false)
const snap = ref<PortStatsSnapshot | null>(null)
const last = ref<Map<number, { t: number; rx: number; tx: number; dropped: number }>>(new Map())
const lastUpdated = ref('')
const refreshMs = ref(2000)
let timer: any

function calcPps(prev: number, cur: number, dtMs: number) {
  if (dtMs <= 0) return 0
  const d = cur - prev
  if (d <= 0) return 0
  return Math.round((d * 1000) / dtMs)
}

async function refresh() {
  loading.value = true
  try {
    const s = await getPortStats()
    const now = Date.now()
    const m = new Map(last.value)
    for (const p of s.ports) {
      const prev = m.get(p.port)
      m.set(p.port, { t: now, rx: p.rx, tx: p.tx, dropped: p.dropped })
      ;(p as any).__prev = prev
      ;(p as any).__now = now
    }
    last.value = m
    snap.value = s
    lastUpdated.value = new Date().toLocaleString()
  } catch (e: any) {
    ElMessage.error(e?.message || '获取端口状态失败')
  } finally {
    loading.value = false
  }
}

const ports = computed(() => {
  const s = snap.value
  if (!s) return []
  const out: Array<PortStatsRow & { rx_pps: number; tx_pps: number; drop_pps: number }> = []
  for (const p of s.ports) {
    const prev = (p as any).__prev as { t: number; rx: number; tx: number; dropped: number } | undefined
    const now = (p as any).__now as number | undefined
    const dt = prev && now ? now - prev.t : 0
    out.push({
      ...p,
      link: p.link || 'unknown',
      duplex: p.duplex || 'unknown',
      rx_pps: prev ? calcPps(prev.rx, p.rx, dt) : 0,
      tx_pps: prev ? calcPps(prev.tx, p.tx, dt) : 0,
      drop_pps: prev ? calcPps(prev.dropped, p.dropped, dt) : 0
    })
  }
  return out.sort((a, b) => a.port - b.port)
})

watch(refreshMs, () => {
  if (timer) clearInterval(timer)
  timer = setInterval(refresh, refreshMs.value)
})

onMounted(() => {
  refresh()
  timer = setInterval(refresh, refreshMs.value)
})
onUnmounted(() => { if (timer) clearInterval(timer) })
</script>

<style scoped>
.topo-head { display:flex; justify-content:space-between; align-items:center; gap:16px; flex-wrap: wrap; }
.topo-title { font-weight: 600; font-size: 16px; }
.topo-actions { display:flex; align-items:center; gap:10px; flex-wrap: wrap; }
.topo-meta { margin-top: 10px; margin-bottom: 12px; color: #909399; font-size: 12px; }
.topo-canvas { display:grid; grid-template-columns: 280px 1fr; gap: 16px; align-items:start; }
.fw-box { border: 1px dashed #c0c4cc; border-radius: 10px; padding: 14px; background: #fafafa; }
.fw-name { font-weight: 600; }
.fw-sub { color: #909399; margin-top: 6px; font-size: 12px; }
.port-grid { display:grid; grid-template-columns: repeat(auto-fill, minmax(240px, 1fr)); gap: 12px; }
.port-card { border: 1px solid #ebeef5; border-radius: 10px; padding: 12px; background: #fff; }
.port-card.up { border-color: #b3e19d; }
.port-card.down { border-color: #fab6b6; }
.port-head { display:flex; justify-content:space-between; align-items:center; margin-bottom: 10px; }
.port-id { font-weight: 600; }
.port-line { display:flex; justify-content:space-between; gap: 12px; margin: 4px 0; }
.k { color: #909399; font-size: 12px; }
.v { font-family: ui-monospace, SFMono-Regular, Menlo, Monaco, Consolas, "Liberation Mono", "Courier New", monospace; font-size: 12px; }
@media (max-width: 1100px) {
  .topo-canvas { grid-template-columns: 1fr; }
}
</style>

