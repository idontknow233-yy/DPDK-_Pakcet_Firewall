<template>
  <div class="atk-layout">
    <div class="atk-left">
      <el-card>
        <div class="atk-head">
          <div class="atk-title">攻击演示与一键防护</div>
          <div class="atk-actions">
            <el-button @click="refresh">刷新</el-button>
            <el-button type="primary" :loading="applying" @click="oneClickDefense">一键防护</el-button>
          </div>
        </div>
        <div class="atk-meta">版本 {{ snap?.version ?? 0 }}，最近更新 {{ lastUpdated || '-' }}</div>

        <div class="atk-kpis">
          <el-card shadow="never" class="kpi">
            <div class="kpi-title">SYN PPS</div>
            <div class="kpi-value">{{ snap?.syn_pps ?? 0 }}</div>
          </el-card>
          <el-card shadow="never" class="kpi">
            <div class="kpi-title">UDP PPS</div>
            <div class="kpi-value">{{ snap?.udp_pps ?? 0 }}</div>
          </el-card>
          <el-card shadow="never" class="kpi">
            <div class="kpi-title">端口扫描事件</div>
            <div class="kpi-value">{{ snap?.scan_events ?? 0 }}</div>
          </el-card>
          <el-card shadow="never" class="kpi">
            <div class="kpi-title">累计封禁</div>
            <div class="kpi-value">{{ snap?.scan_banned ?? 0 }}</div>
          </el-card>
        </div>

        <el-divider />
        <div class="atk-subtitle">Top 扫描源</div>
        <el-table :data="topRows" size="small" stripe v-loading="applying" style="width:100%">
          <el-table-column prop="family" label="IP版本" width="90" />
          <el-table-column prop="ip" label="源地址" min-width="260" />
          <el-table-column prop="ports" label="端口数/秒" width="120" />
        </el-table>
      </el-card>
    </div>

    <div class="atk-right">
      <el-card>
        <div class="atk-subtitle">防护设置</div>
        <el-form label-width="120px">
          <el-form-item label="扫描封禁">
            <el-switch v-model="mitigation" :active-value="1" :inactive-value="0" />
          </el-form-item>
          <el-form-item label="阈值(端口/秒)">
            <el-input-number v-model="scanPortsSec" :min="1" :max="1000" />
          </el-form-item>
          <el-form-item label="封禁秒数">
            <el-input-number v-model="banSec" :min="1" :max="3600" />
          </el-form-item>
          <el-form-item>
            <el-button type="primary" :loading="applying" @click="apply">应用</el-button>
          </el-form-item>
        </el-form>
        <el-divider />
        <div class="atk-subtitle">演示建议</div>
        <div class="atk-tips">
          <div>1) 用 nmap/自写脚本对目标做端口扫描</div>
          <div>2) 页面观察端口扫描事件增加，Top 扫描源出现</div>
          <div>3) 开启“一键防护”，再扫同一源将被快速封禁</div>
          <div>4) SYN/UDP 洪泛可配合 DDoS/限速页一起展示</div>
        </div>
      </el-card>
    </div>
  </div>
</template>

<script lang="ts" setup>
import { computed, onMounted, onUnmounted, ref } from 'vue'
import { ElMessage } from 'element-plus'
import { getAttack, setAttack, setDdos, type AttackSnapshot } from '../services/api'

const loading = ref(false)
const applying = ref(false)
const snap = ref<AttackSnapshot | null>(null)
const lastUpdated = ref('')
const mitigation = ref(0)
const scanPortsSec = ref(50)
const banSec = ref(60)
let timer: any

const topRows = computed(() => {
  const s = snap.value
  if (!s) return []
  return [
    { family: 'IPv4', ip: s.top4 || '-', ports: s.top4_ports || 0 },
    { family: 'IPv6', ip: s.top6 || '-', ports: s.top6_ports || 0 }
  ]
})

async function refresh() {
  try {
    const s = await getAttack()
    snap.value = s
    mitigation.value = s.mitigation
    scanPortsSec.value = s.scan_ports_sec
    banSec.value = s.ban_sec
    lastUpdated.value = new Date().toLocaleString()
  } catch (e: any) {
    ElMessage.error(e?.message || '获取攻击状态失败')
  }
}

async function apply() {
  applying.value = true
  try {
    await setAttack({ mitigation: mitigation.value, scan_ports_sec: scanPortsSec.value, ban_sec: banSec.value })
    ElMessage.success('已应用')
    await refresh()
  } catch (e: any) {
    ElMessage.error(e?.message || '应用失败')
  } finally {
    applying.value = false
  }
}

async function oneClickDefense() {
  applying.value = true
  try {
    await setDdos({ syn_pps: 5000, syn_burst: 2000, udp_pps: 20000, udp_burst: 5000 })
    await setAttack({ mitigation: 1, scan_ports_sec: 30, ban_sec: 120 })
    ElMessage.success('已开启一键防护')
    await refresh()
  } catch (e: any) {
    ElMessage.error(e?.message || '一键防护失败')
  } finally {
    applying.value = false
  }
}

onMounted(() => {
  refresh()
  timer = setInterval(refresh, 2000)
})
onUnmounted(() => { if (timer) clearInterval(timer) })
</script>

<style scoped>
.atk-layout { display:flex; gap:16px; align-items:flex-start; flex-wrap: wrap; }
.atk-left { flex: 1; min-width: 620px; }
.atk-right { width: 420px; }
.atk-head { display:flex; justify-content:space-between; align-items:center; gap:16px; flex-wrap: wrap; }
.atk-title { font-weight: 600; font-size: 16px; }
.atk-actions { display:flex; gap:10px; flex-wrap: wrap; }
.atk-meta { margin-top: 10px; margin-bottom: 12px; color: #909399; font-size: 12px; }
.atk-kpis { display:grid; grid-template-columns: repeat(4, minmax(0, 1fr)); gap: 12px; }
.kpi { padding: 6px; }
.kpi-title { color:#909399; font-size: 12px; }
.kpi-value { font-weight: 700; font-size: 22px; margin-top: 6px; }
.atk-subtitle { font-weight: 600; margin-bottom: 10px; }
.atk-tips { color:#606266; font-size: 13px; line-height: 1.8; }
@media (max-width: 1100px) {
  .atk-right { width: 100%; }
  .atk-left { min-width: 0; width: 100%; }
  .atk-kpis { grid-template-columns: repeat(2, minmax(0, 1fr)); }
}
</style>

