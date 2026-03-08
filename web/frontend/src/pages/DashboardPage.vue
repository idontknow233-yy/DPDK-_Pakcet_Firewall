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
    </div>
    <el-card>
      <div class="dash-chart__head">
        <div class="dash-chart__title">规则数量趋势</div>
        <div class="dash-chart__meta">最近更新：{{ lastUpdated || '-' }}</div>
      </div>
      <div id="chart" class="dash-chart"></div>
    </el-card>
  </div>
</template>

<script lang="ts" setup>
import { onMounted, onUnmounted, ref } from 'vue'
import * as echarts from 'echarts'
import { getAclSnapshot, getHealth } from '../services/api'

const healthy = ref(false)
const count = ref(0)
const version = ref(0)
const loading = ref(false)
const lastUpdated = ref('')
let timer: any
let chart: echarts.ECharts | null = null
const xs: string[] = []
const ys: number[] = []

async function refresh() {
  loading.value = true
  try {
    healthy.value = await getHealth()
    const snap = await getAclSnapshot()
    count.value = snap.count
    version.value = snap.version
    const t = new Date().toLocaleTimeString()
    xs.push(t)
    ys.push(snap.count)
    if (xs.length > 24) {
      xs.shift()
      ys.shift()
    }
    lastUpdated.value = new Date().toLocaleString()
    chart?.setOption({
      tooltip: { trigger: 'axis' },
      grid: { left: 32, right: 16, top: 20, bottom: 24 },
      xAxis: { type: 'category', data: xs, boundaryGap: false },
      yAxis: { type: 'value', minInterval: 1 },
      series: [{ name: 'rules', type: 'line', data: ys, smooth: true, showSymbol: false, areaStyle: { opacity: 0.08 } }]
    })
  } finally {
    loading.value = false
  }
}

onMounted(() => {
  chart = echarts.init(document.getElementById('chart') as HTMLElement)
  chart.setOption({
    tooltip: { trigger: 'axis' },
    grid: { left: 32, right: 16, top: 20, bottom: 24 },
    xAxis: { type: 'category', data: [], boundaryGap: false },
    yAxis: { type: 'value', minInterval: 1 },
    series: [{ type: 'line', data: [], smooth: true, showSymbol: false }]
  })
  refresh()
  timer = setInterval(refresh, 5000)
  window.addEventListener('resize', onResize)
})
function onResize() {
  chart?.resize()
}
onUnmounted(() => {
  if (timer) clearInterval(timer)
  window.removeEventListener('resize', onResize)
  if (chart) chart.dispose()
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
