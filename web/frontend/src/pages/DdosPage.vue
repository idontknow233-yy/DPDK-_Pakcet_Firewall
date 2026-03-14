<template>
  <div class="ddos-layout">
    <el-card>
      <div class="ddos-head">
        <div class="ddos-title">抗DDoS/限速配置</div>
        <div class="ddos-actions">
          <el-button :loading="loading" @click="refresh">刷新</el-button>
          <el-button type="primary" :loading="saving" @click="apply">应用配置</el-button>
        </div>
      </div>
      <div class="ddos-meta">修改后约 10 秒内生效（取决于数据面定时器周期）</div>
      <div class="ddos-grid">
        <div class="ddos-item">
          <div class="ddos-label">SYN PPS</div>
          <el-input-number v-model="synPps" :min="0" :max="2000000000" controls-position="right" />
        </div>
        <div class="ddos-item">
          <div class="ddos-label">SYN Burst</div>
          <el-input-number v-model="synBurst" :min="0" :max="2000000000" controls-position="right" />
        </div>
        <div class="ddos-item">
          <div class="ddos-label">UDP PPS</div>
          <el-input-number v-model="udpPps" :min="0" :max="2000000000" controls-position="right" />
        </div>
        <div class="ddos-item">
          <div class="ddos-label">UDP Burst</div>
          <el-input-number v-model="udpBurst" :min="0" :max="2000000000" controls-position="right" />
        </div>
      </div>
      <div class="ddos-tips">
        <div class="ddos-tip">关闭限速：将对应 PPS 设为 0（Burst 会被忽略）</div>
        <div class="ddos-tip">建议：Burst ≥ PPS，用于容纳短时突发</div>
      </div>
    </el-card>
  </div>
</template>

<script lang="ts" setup>
import { ref } from 'vue'
import { ElMessage } from 'element-plus'
import { getDdos, setDdos } from '../services/api'

const loading = ref(false)
const saving = ref(false)
const synPps = ref(0)
const synBurst = ref(0)
const udpPps = ref(0)
const udpBurst = ref(0)

async function refresh() {
  loading.value = true
  try {
    const cfg = await getDdos()
    synPps.value = cfg.syn_pps
    synBurst.value = cfg.syn_burst
    udpPps.value = cfg.udp_pps
    udpBurst.value = cfg.udp_burst
  } catch (e: any) {
    ElMessage.error(e?.message || '获取配置失败')
  } finally {
    loading.value = false
  }
}

async function apply() {
  saving.value = true
  try {
    await setDdos({
      syn_pps: synPps.value,
      syn_burst: synBurst.value,
      udp_pps: udpPps.value,
      udp_burst: udpBurst.value
    })
    ElMessage.success('已应用')
    await refresh()
  } catch (e: any) {
    ElMessage.error(e?.message || '应用失败')
  } finally {
    saving.value = false
  }
}

refresh()
</script>

<style scoped>
.ddos-layout { max-width: 960px; }
.ddos-head { display:flex; align-items:center; justify-content:space-between; gap: 16px; flex-wrap: wrap; }
.ddos-title { font-weight: 600; font-size: 16px; }
.ddos-actions { display:flex; gap: 10px; align-items:center; }
.ddos-meta { margin-top: 10px; color: #909399; font-size: 12px; }
.ddos-grid { margin-top: 12px; display:flex; gap: 16px; flex-wrap: wrap; }
.ddos-item { min-width: 220px; }
.ddos-label { color: #909399; margin-bottom: 6px; font-size: 12px; }
.ddos-tips { margin-top: 12px; color: #909399; font-size: 12px; display:flex; flex-direction: column; gap: 6px; }
</style>

