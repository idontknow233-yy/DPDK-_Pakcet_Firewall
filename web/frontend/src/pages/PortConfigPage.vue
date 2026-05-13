<template>
  <div class="pc-layout">
    <div class="pc-left">
      <el-card>
        <div class="pc-head">
          <div class="pc-title">IPv4 接口配置</div>
          <div class="pc-actions">
            <el-button :loading="loading" @click="refresh">刷新</el-button>
          </div>
        </div>
        <div class="pc-meta">版本 {{ ifcfg4.version }}，总数 {{ ifcfg4.count }}</div>
        <el-table :data="ifcfg4.ifaces" v-loading="loading" stripe style="width: 100%">
          <el-table-column prop="port" label="端口" width="90" sortable />
          <el-table-column prop="cidr" label="IPv4/CIDR" min-width="240" />
          <el-table-column label="操作" width="120">
            <template #default="{row}">
              <el-button type="danger" link @click="clearIface(row.port)">清除</el-button>
            </template>
          </el-table-column>
        </el-table>
        <div class="pc-form">
          <el-form inline>
            <el-form-item label="端口">
              <el-input-number v-model="ifPort" :min="0" :max="31" />
            </el-form-item>
            <el-form-item label="IPv4/CIDR">
              <el-input v-model="ifCidr" placeholder="例如 192.168.1.1/24" style="width: 200px" />
            </el-form-item>
            <el-form-item>
              <el-button type="primary" :loading="loading" @click="setIface">设置</el-button>
            </el-form-item>
          </el-form>
        </div>
      </el-card>
    </div>
    <div class="pc-right">
      <el-card>
        <div class="pc-head">
          <div class="pc-title">IPv6 接口配置</div>
          <div class="pc-actions">
            <el-button :loading="loading" @click="refresh">刷新</el-button>
          </div>
        </div>
        <div class="pc-meta">版本 {{ ifcfg6.version }}，总数 {{ ifcfg6.count }}</div>
        <el-table :data="ifcfg6.ifaces" v-loading="loading" stripe style="width: 100%">
          <el-table-column prop="port" label="端口" width="90" sortable />
          <el-table-column prop="cidr" label="IPv6/CIDR" min-width="240" />
          <el-table-column label="操作" width="120">
            <template #default="{row}">
              <el-button type="danger" link @click="clearIface6(row.port)">清除</el-button>
            </template>
          </el-table-column>
        </el-table>
        <div class="pc-form">
          <el-form inline>
            <el-form-item label="端口">
              <el-input-number v-model="ifPort6" :min="0" :max="31" />
            </el-form-item>
            <el-form-item label="IPv6/CIDR">
              <el-input v-model="ifCidr6" placeholder="例如 2001:db8:1::1/64" style="width: 260px" />
            </el-form-item>
            <el-form-item>
              <el-button type="primary" :loading="loading" @click="setIface6">设置</el-button>
            </el-form-item>
          </el-form>
        </div>
      </el-card>
    </div>
  </div>
</template>

<script lang="ts" setup>
import { onMounted, ref } from 'vue'
import { ElMessage, ElMessageBox } from 'element-plus'
import { getIfcfg4, setIfcfg4, clearIfcfg4, getIfcfg6, setIfcfg6, clearIfcfg6 } from '../services/api'

const loading = ref(false)
const ifcfg4 = ref<{ version: number; count: number; ifaces: { port: number; cidr: string }[] }>({ version: 0, count: 0, ifaces: [] })
const ifcfg6 = ref<{ version: number; count: number; ifaces: { port: number; cidr: string }[] }>({ version: 0, count: 0, ifaces: [] })

const ifPort = ref(0)
const ifCidr = ref('')
const ifPort6 = ref(0)
const ifCidr6 = ref('')

async function refresh() {
  loading.value = true
  try {
    const [a, b] = await Promise.all([
      getIfcfg4().catch(() => ({ version: 0, count: 0, ifaces: [] })),
      getIfcfg6().catch(() => ({ version: 0, count: 0, ifaces: [] }))
    ])
    ifcfg4.value = a
    ifcfg6.value = b
  } catch (e: any) {
    ElMessage.error(e?.message || '获取配置失败')
  } finally {
    loading.value = false
  }
}

async function setIface() {
  if (!ifCidr.value.trim()) {
    ElMessage.warning('请输入 IPv4/CIDR')
    return
  }
  loading.value = true
  try {
    await setIfcfg4(ifPort.value, ifCidr.value)
    ElMessage.success('已设置')
    await refresh()
  } catch (e: any) {
    ElMessage.error(e?.message || '设置失败')
  } finally {
    loading.value = false
  }
}

async function clearIface(port: number) {
  try {
    await ElMessageBox.confirm(`确认清除端口 ${port} 的 IPv4 配置？`, '提示', { type: 'warning' })
    loading.value = true
    await clearIfcfg4(port)
    ElMessage.success('已清除')
    await refresh()
  } catch {}
  finally { loading.value = false }
}

async function setIface6() {
  if (!ifCidr6.value.trim()) {
    ElMessage.warning('请输入 IPv6/CIDR')
    return
  }
  loading.value = true
  try {
    await setIfcfg6(ifPort6.value, ifCidr6.value)
    ElMessage.success('已设置')
    await refresh()
  } catch (e: any) {
    ElMessage.error(e?.message || '设置失败')
  } finally {
    loading.value = false
  }
}

async function clearIface6(port: number) {
  try {
    await ElMessageBox.confirm(`确认清除端口 ${port} 的 IPv6 配置？`, '提示', { type: 'warning' })
    loading.value = true
    await clearIfcfg6(port)
    ElMessage.success('已清除')
    await refresh()
  } catch {}
  finally { loading.value = false }
}

onMounted(() => refresh())
</script>

<style scoped>
.pc-layout { display:flex; gap:16px; align-items:flex-start; flex-wrap: wrap; }
.pc-left { flex:1; min-width: 520px; }
.pc-right { flex:1; min-width: 520px; }
.pc-head { display:flex; justify-content:space-between; align-items:center; gap: 16px; }
.pc-title { font-weight: 600; font-size: 16px; }
.pc-meta { margin-top: 10px; margin-bottom: 10px; color:#909399; font-size: 12px; }
.pc-form { margin-top: 14px; }
@media (max-width: 1100px) {
  .pc-left, .pc-right { min-width: 0; width: 100%; }
}
</style>
