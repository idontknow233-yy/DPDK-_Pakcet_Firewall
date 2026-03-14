<template>
  <div class="acl-layout">
    <div class="acl-left">
      <el-card>
        <div class="acl-toolbar">
          <div class="acl-toolbar__title">
            <div class="acl-title">IPv6 ACL规则</div>
            <el-tag effect="plain">版本 {{ version }}</el-tag>
            <el-tag effect="plain">总数 {{ count }}</el-tag>
          </div>
          <div class="acl-toolbar__actions">
            <el-input v-model="q" placeholder="搜索 src/dst" clearable style="width: 220px" />
            <el-select v-model="actionFilter" clearable placeholder="动作" style="width: 120px">
              <el-option label="allow" value="allow" />
              <el-option label="deny" value="deny" />
            </el-select>
            <el-select v-model="protoFilter" clearable placeholder="协议" style="width: 120px">
              <el-option label="any" value="any" />
              <el-option label="TCP" value="6" />
              <el-option label="UDP" value="17" />
            </el-select>
            <el-button :loading="loading" @click="refresh">刷新</el-button>
            <el-button type="danger" :loading="loading" @click="clearAll">清空</el-button>
          </div>
        </div>
        <div class="acl-meta">最近更新：{{ lastUpdated || '-' }}</div>
        <rule-table :rows="filteredRows" :loading="loading" @delete="deleteOne" />
      </el-card>
    </div>
    <div class="acl-right">
      <el-card>
        <div class="acl-right__title">新增规则</div>
        <rule-form :key="formKey" :submitting="submitting" :ip-version="6" @submit="addRule" />
      </el-card>
    </div>
  </div>
</template>

<script lang="ts" setup>
import { computed, ref } from 'vue'
import RuleForm from '../components/RuleForm.vue'
import RuleTable from '../components/RuleTable.vue'
import { addAcl6Rule, clearAcl6, deleteAcl6Rule, getAcl6Snapshot } from '../services/api'
import { ElMessageBox, ElMessage } from 'element-plus'

const version = ref(0)
const count = ref(0)
const rows = ref<any[]>([])
const loading = ref(false)
const submitting = ref(false)
const lastUpdated = ref('')
const q = ref('')
const actionFilter = ref<string | undefined>()
const protoFilter = ref<string | undefined>()
const formKey = ref(0)

function mapRows(rules: any[]) { return rules }

async function refresh() {
  loading.value = true
  try {
    const snap = await getAcl6Snapshot()
    version.value = snap.version
    count.value = snap.count
    rows.value = mapRows(snap.rules)
    lastUpdated.value = new Date().toLocaleString()
  } catch (e: any) {
    ElMessage.error(e?.message || '获取ACL失败')
  } finally {
    loading.value = false
  }
}

const filteredRows = computed(() => {
  const kw = q.value.trim().toLowerCase()
  return rows.value.filter((r) => {
    if (actionFilter.value && r.action !== actionFilter.value) return false
    if (protoFilter.value) {
      const p = String(r.proto)
      if (p !== protoFilter.value) return false
    }
    if (!kw) return true
    return String(r.src).toLowerCase().includes(kw) || String(r.dst).toLowerCase().includes(kw)
  })
})

async function addRule(payload: any) {
  submitting.value = true
  try {
    const action = payload.allow === 1 ? 'allow' : 'deny'
    await addAcl6Rule({
      action,
      src: payload.src,
      dst: payload.dst,
      proto: payload.proto,
      src_port_min: payload.src_port_min,
      src_port_max: payload.src_port_max,
      dst_port_min: payload.dst_port_min,
      dst_port_max: payload.dst_port_max
    })
    ElMessage.success('已新增')
    formKey.value++
    await refresh()
  } catch (e: any) {
    ElMessage.error(e?.message || '新增失败')
  } finally {
    submitting.value = false
  }
}

async function deleteOne(index: number) {
  try {
    await ElMessageBox.confirm('确认删除该规则？', '提示', { type: 'warning' })
    loading.value = true
    await deleteAcl6Rule(index)
    ElMessage.success('已删除')
    await refresh()
  } catch {}
  finally { loading.value = false }
}

async function clearAll() {
  try {
    await ElMessageBox.confirm('确认清空所有规则？', '警告', { type: 'warning' })
    loading.value = true
    await clearAcl6()
    ElMessage.success('已清空')
    await refresh()
  } catch {}
  finally { loading.value = false }
}

refresh()
</script>

<style scoped>
.acl-layout { display:flex; gap:16px; align-items:flex-start; }
.acl-left { flex:1; min-width: 620px; }
.acl-right { width: 420px; }
.acl-toolbar { display:flex; justify-content:space-between; align-items:center; gap:16px; flex-wrap: wrap; }
.acl-toolbar__title { display:flex; align-items:center; gap:10px; flex-wrap: wrap; }
.acl-title { font-weight: 600; font-size: 16px; }
.acl-toolbar__actions { display:flex; align-items:center; gap:10px; flex-wrap: wrap; }
.acl-meta { margin-top: 10px; margin-bottom: 10px; color: #909399; font-size: 12px; }
.acl-right__title { font-weight: 600; margin-bottom: 12px; }
@media (max-width: 1100px) {
  .acl-layout { flex-direction: column; }
  .acl-right { width: 100%; }
  .acl-left { min-width: 0; width: 100%; }
}
</style>

