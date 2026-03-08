<template>
  <div>
    <el-card>
      <div class="sess-toolbar">
        <div class="sess-title">
          <div class="sess-title__name">会话表</div>
          <el-tag effect="plain">版本 {{ version }}</el-tag>
          <el-tag effect="plain">总数 {{ count }}</el-tag>
        </div>
        <div class="sess-actions">
          <el-select v-model="proto" clearable placeholder="协议" style="width:120px">
            <el-option label="TCP" :value="6" />
            <el-option label="UDP" :value="17" />
          </el-select>
          <el-input v-model="q" placeholder="搜索 src/dst" clearable style="width:240px" />
          <el-select v-model="limit" style="width:140px">
            <el-option :value="100" label="100条" />
            <el-option :value="200" label="200条" />
            <el-option :value="500" label="500条" />
            <el-option :value="1000" label="1000条" />
          </el-select>
          <el-button :loading="loading" @click="refresh">刷新</el-button>
        </div>
      </div>
      <div class="sess-meta">最近更新：{{ lastUpdated || '-' }}</div>
      <el-table :data="filtered" v-loading="loading" stripe style="width:100%">
        <el-table-column prop="index" label="#" width="72" sortable />
        <el-table-column label="协议" width="90">
          <template #default="{row}">
            <el-tag :type="row.proto === 6 ? 'success' : 'warning'" effect="light">
              {{ row.proto === 6 ? 'TCP' : row.proto === 17 ? 'UDP' : row.proto }}
            </el-tag>
          </template>
        </el-table-column>
        <el-table-column prop="src" label="源" min-width="220" />
        <el-table-column prop="dst" label="目的" min-width="220" />
        <el-table-column prop="packets" label="包数" width="120" sortable />
        <el-table-column prop="bytes" label="字节" width="140" sortable />
        <el-table-column label="最近活跃" width="140" sortable>
          <template #default="{row}">
            <span>{{ formatAge(row.last_seen_ms) }}</span>
          </template>
        </el-table-column>
      </el-table>
    </el-card>
  </div>
</template>

<script lang="ts" setup>
import { computed, onMounted, onUnmounted, ref, watch } from 'vue'
import { ElMessage } from 'element-plus'
import { getSessions, type SessionRow } from '../services/api'

const version = ref(0)
const count = ref(0)
const rows = ref<SessionRow[]>([])
const loading = ref(false)
const lastUpdated = ref('')
const q = ref('')
const proto = ref<number | undefined>()
const limit = ref(200)
let timer: any

function formatAge(ms: number) {
  if (!ms) return '-'
  if (ms < 1000) return `${ms}ms`
  const s = Math.floor(ms / 1000)
  if (s < 60) return `${s}s`
  const m = Math.floor(s / 60)
  if (m < 60) return `${m}m`
  const h = Math.floor(m / 60)
  return `${h}h`
}

const filtered = computed(() => {
  const kw = q.value.trim().toLowerCase()
  return rows.value.filter((r) => {
    if (proto.value && r.proto !== proto.value) return false
    if (!kw) return true
    return r.src.toLowerCase().includes(kw) || r.dst.toLowerCase().includes(kw)
  })
})

async function refresh() {
  loading.value = true
  try {
    const snap = await getSessions(limit.value)
    version.value = snap.version
    count.value = snap.count
    rows.value = snap.sessions
    lastUpdated.value = new Date().toLocaleString()
  } catch (e: any) {
    ElMessage.error(e?.message || '获取会话失败')
  } finally {
    loading.value = false
  }
}

watch(limit, () => refresh())

onMounted(() => {
  refresh()
  timer = setInterval(refresh, 5000)
})
onUnmounted(() => { if (timer) clearInterval(timer) })
</script>

<style scoped>
.sess-toolbar { display:flex; justify-content:space-between; align-items:center; gap:16px; flex-wrap: wrap; }
.sess-title { display:flex; align-items:center; gap:10px; flex-wrap: wrap; }
.sess-title__name { font-weight: 600; font-size: 16px; }
.sess-actions { display:flex; align-items:center; gap:10px; flex-wrap: wrap; }
.sess-meta { margin-top: 10px; margin-bottom: 10px; color: #909399; font-size: 12px; }
</style>

