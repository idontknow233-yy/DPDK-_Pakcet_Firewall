<template>
  <div>
    <div class="dash-cards">
      <el-card class="dash-card" v-loading="loading">
        <div class="dash-card__label">控制面健康</div>
        <div class="dash-card__value">
          <el-tag :type="healthy ? 'success' : 'danger'" effect="dark">
            {{ healthy ? 'Healthy' : 'Unhealthy' }}
          </el-tag>
        </div>
      </el-card>
      <el-card class="dash-card" v-loading="loading">
        <div class="dash-card__label">规则数量</div>
        <div class="dash-card__value">{{ count }}</div>
      </el-card>
      <el-card class="dash-card" v-loading="loading">
        <div class="dash-card__label">规则版本</div>
        <div class="dash-card__value">{{ version }}</div>
      </el-card>
      <el-card class="dash-card" v-loading="loading">
        <div class="dash-card__label">RX PPS</div>
        <div class="dash-card__value">{{ rxPps }}</div>
      </el-card>
      <el-card class="dash-card" v-loading="loading">
        <div class="dash-card__label">TX PPS</div>
        <div class="dash-card__value">{{ txPps }}</div>
      </el-card>
      <el-card class="dash-card" v-loading="loading">
        <div class="dash-card__label">DROP PPS</div>
        <div class="dash-card__value">{{ dropPps }}</div>
      </el-card>
    </div>
    <el-card>
      <div class="dash-chart__head">
        <div class="dash-chart__title">规则数量趋势</div>
        <div class="dash-chart__meta">最近更新：{{ lastUpdated || '-' }}</div>
      </div>
      <div id="rules-chart" class="dash-chart"></div>
    </el-card>
    <el-card style="margin-top: 16px;">
      <div class="dash-chart__head">
        <div class="dash-chart__title">流量趋势</div>
        <div class="dash-chart__meta">最近更新：{{ lastUpdated || '-' }}</div>
      </div>
      <div id="traffic-chart" class="dash-chart"></div>
    </el-card>
    <el-card style="margin-top: 16px;">
      <div class="dash-chart__head">
        <div class="dash-chart__title">最近拦截</div>
        <div class="dash-chart__meta">最近更新：{{ lastUpdated || '-' }}</div>
      </div>
      <el-tabs v-model="denyTab" style="margin-bottom: 8px;">
        <el-tab-pane label="IPv4" name="4" />
        <el-tab-pane label="IPv6" name="6" />
      </el-tabs>
      <el-table :data="denies" size="small" style="width: 100%">
        <el-table-column prop="age_ms" label="age(ms)" width="100" />
        <el-table-column prop="in_port" label="in_port" width="80" />
        <el-table-column prop="proto" label="proto" width="70" />
        <el-table-column prop="src" label="src" />
        <el-table-column prop="dst" label="dst" />
        <el-table-column prop="rule" label="rule" width="80" />
      </el-table>
    </el-card>
  </div>
</template>

<script lang="ts" setup>
import { onMounted, onUnmounted, ref } from 'vue'
import * as echarts from 'echarts'
import { computed } from 'vue'
import { type DenyRow, getAclSnapshot, getDenies, getDenies6, getHealth, getPortStats } from '../services/api'

const healthy = ref(false)
const count = ref(0)
const version = ref(0)
const loading = ref(false)
const lastUpdated = ref('')
const rxPps = ref(0)
const txPps = ref(0)
const dropPps = ref(0)
const denyTab = ref<'4' | '6'>('4')
const denies4 = ref<DenyRow[]>([])
const denies6 = ref<DenyRow[]>([])
const denies = computed(() => denyTab.value === '6' ? denies6.value : denies4.value)
let timer: any
let chartRules: echarts.ECharts | null = null
let chartTraffic: echarts.ECharts | null = null
const rulesXs: string[] = []
const rulesYs: number[] = []
const trafficXs: string[] = []
const trafficRx: number[] = []
const trafficTx: number[] = []
const trafficDrop: number[] = []
let lastPortTotals: { rx: number; tx: number; dropped: number; t: number } | null = null

async function refresh() {
  loading.value = true
  try {
    healthy.value = await getHealth()
    const snap = await getAclSnapshot()
    count.value = snap.count
    version.value = snap.version
    const t = new Date().toLocaleTimeString()
    rulesXs.push(t)
    rulesYs.push(snap.count)
    if (rulesXs.length > 24) {
      rulesXs.shift()
      rulesYs.shift()
    }
    const ps = await getPortStats()
    let rx = 0
    let tx = 0
    let dropped = 0
    for (const p of ps.ports) {
      rx += p.rx
      tx += p.tx
      dropped += p.dropped
    }
    const nowMs = Date.now()
    if (lastPortTotals) {
      const dt = (nowMs - lastPortTotals.t) / 1000
      if (dt > 0) {
        rxPps.value = Math.max(0, Math.round((rx - lastPortTotals.rx) / dt))
        txPps.value = Math.max(0, Math.round((tx - lastPortTotals.tx) / dt))
        dropPps.value = Math.max(0, Math.round((dropped - lastPortTotals.dropped) / dt))
      }
    }
    lastPortTotals = { rx, tx, dropped, t: nowMs }
    trafficXs.push(t)
    trafficRx.push(rxPps.value)
    trafficTx.push(txPps.value)
    trafficDrop.push(dropPps.value)
    if (trafficXs.length > 24) {
      trafficXs.shift()
      trafficRx.shift()
      trafficTx.shift()
      trafficDrop.shift()
    }
    const ds = await getDenies(20)
    denies4.value = ds.denies
    try {
      const ds6 = await getDenies6(20)
      denies6.value = ds6.denies
    } catch {}
    lastUpdated.value = new Date().toLocaleString()
    chartRules?.setOption({
      tooltip: { trigger: 'axis' },
      grid: { left: 32, right: 16, top: 20, bottom: 24 },
      xAxis: { type: 'category', data: rulesXs, boundaryGap: false },
      yAxis: { type: 'value', minInterval: 1 },
      series: [{ name: 'rules', type: 'line', data: rulesYs, smooth: true, showSymbol: false, areaStyle: { opacity: 0.08 } }]
    })
    chartTraffic?.setOption({
      tooltip: { trigger: 'axis' },
      legend: { top: 0 },
      grid: { left: 48, right: 16, top: 28, bottom: 24 },
      xAxis: { type: 'category', data: trafficXs, boundaryGap: false },
      yAxis: { type: 'value', minInterval: 1 },
      series: [
        { name: 'rx_pps', type: 'line', data: trafficRx, smooth: true, showSymbol: false },
        { name: 'tx_pps', type: 'line', data: trafficTx, smooth: true, showSymbol: false },
        { name: 'drop_pps', type: 'line', data: trafficDrop, smooth: true, showSymbol: false }
      ]
    })
  } finally {
    loading.value = false
  }
}

onMounted(() => {
  chartRules = echarts.init(document.getElementById('rules-chart') as HTMLElement)
  chartRules.setOption({
    tooltip: { trigger: 'axis' },
    grid: { left: 32, right: 16, top: 20, bottom: 24 },
    xAxis: { type: 'category', data: [], boundaryGap: false },
    yAxis: { type: 'value', minInterval: 1 },
    series: [{ type: 'line', data: [], smooth: true, showSymbol: false }]
  })
  chartTraffic = echarts.init(document.getElementById('traffic-chart') as HTMLElement)
  chartTraffic.setOption({
    tooltip: { trigger: 'axis' },
    legend: { top: 0 },
    grid: { left: 48, right: 16, top: 28, bottom: 24 },
    xAxis: { type: 'category', data: [], boundaryGap: false },
    yAxis: { type: 'value', minInterval: 1 },
    series: [
      { name: 'rx_pps', type: 'line', data: [], smooth: true, showSymbol: false },
      { name: 'tx_pps', type: 'line', data: [], smooth: true, showSymbol: false },
      { name: 'drop_pps', type: 'line', data: [], smooth: true, showSymbol: false }
    ]
  })
  refresh()
  timer = setInterval(refresh, 5000)
  window.addEventListener('resize', onResize)
})
function onResize() {
  chartRules?.resize()
  chartTraffic?.resize()
}
onUnmounted(() => {
  if (timer) clearInterval(timer)
  window.removeEventListener('resize', onResize)
  if (chartRules) chartRules.dispose()
  if (chartTraffic) chartTraffic.dispose()
})
</script>

<style scoped>
.dash-cards { display:flex; gap:16px; margin-bottom:16px; flex-wrap: wrap; }
.dash-card { flex: 1; min-width: 220px; }
.dash-card__label { color: #909399; }
.dash-card__value { margin-top: 10px; font-size: 22px; font-weight: 700; color: #303133; }
.dash-chart__head { display:flex; align-items:center; justify-content:space-between; margin-bottom: 8px; }
.dash-chart__title { font-weight: 600; }
.dash-chart__meta { color: #909399; font-size: 12px; }
.dash-chart { height: 320px; }
</style>
