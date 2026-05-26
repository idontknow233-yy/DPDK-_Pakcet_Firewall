<template>
  <div class="r4-layout">
    <div class="r4-left">
      <el-card>
        <div class="r4-head">
          <div class="r4-title">IPv4 接口配置</div>
          <div class="r4-actions">
            <el-button :loading="loading" @click="refresh">刷新</el-button>
          </div>
        </div>
        <div class="r4-meta">版本 {{ ifcfg4.version }}，总数 {{ ifcfg4.count }}</div>
        <el-table :data="ifcfg4.ifaces" v-loading="loading" stripe style="width: 100%">
          <el-table-column prop="port" label="端口" width="90" sortable />
          <el-table-column prop="cidr" label="IPv4/CIDR" min-width="220" />
          <el-table-column label="操作" width="120">
            <template #default="{row}">
              <el-button type="danger" link @click="clearIface(row.port)">清除</el-button>
            </template>
          </el-table-column>
        </el-table>
        <div class="r4-form">
          <el-form inline>
            <el-form-item label="端口">
              <el-input-number v-model="ifPort" :min="0" :max="31" />
            </el-form-item>
            <el-form-item label="IPv4/CIDR">
              <el-input v-model="ifCidr" placeholder="例如 192.168.1.1/24" style="width: 240px" />
            </el-form-item>
            <el-form-item>
              <el-button type="primary" :loading="loading" @click="setIface">设置</el-button>
            </el-form-item>
          </el-form>
        </div>
      </el-card>
    </div>
    <div class="r4-right">
      <el-card>
        <div class="r4-head">
          <div class="r4-title">IPv4 路由表</div>
          <div class="r4-actions">
            <el-button type="danger" :loading="loading" @click="clearRoutes">清空</el-button>
          </div>
        </div>
        <div class="r4-meta">版本 {{ route4.version }}，总数 {{ route4.count }}</div>
        <el-table :data="route4.routes" v-loading="loading" stripe style="width: 100%">
          <el-table-column prop="index" label="#" width="80" sortable />
          <el-table-column prop="dst" label="目的前缀" min-width="220" />
          <el-table-column prop="nh" label="下一跳" min-width="200" />
          <el-table-column prop="port" label="出端口" width="110" />
          <el-table-column label="操作" width="120">
            <template #default="{row}">
              <el-button type="danger" link @click="deleteRoute(row.index)">删除</el-button>
            </template>
          </el-table-column>
        </el-table>
        <div class="r4-form">
          <el-form inline>
            <el-form-item label="目的">
              <el-input v-model="rtDst" placeholder="例如 10.0.0.0/8" style="width: 220px" />
            </el-form-item>
            <el-form-item label="下一跳">
              <el-input v-model="rtNh" placeholder="例如 192.168.1.1 或留空(0.0.0.0)" style="width: 220px" />
            </el-form-item>
            <el-form-item label="端口">
              <el-input-number v-model="rtPort" :min="0" :max="31" />
            </el-form-item>
            <el-form-item>
              <el-button type="primary" :loading="loading" @click="addRoute">新增</el-button>
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
import { addRoute4, clearIfcfg4, clearRoute4, deleteRoute4, getIfcfg4, getRoute4, setIfcfg4 } from '../services/api'

const loading = ref(false)
const ifcfg4 = ref<{ version: number; count: number; ifaces: { port: number; cidr: string }[] }>({ version: 0, count: 0, ifaces: [] })
const route4 = ref<{ version: number; count: number; routes: { index: number; dst: string; nh: string; port: number }[] }>({ version: 0, count: 0, routes: [] })

const ifPort = ref(0)
const ifCidr = ref('')

const rtDst = ref('')
const rtNh = ref('')
const rtPort = ref(0)

async function refresh() {
  loading.value = true
  try {
    const a = await getIfcfg4()
    const b = await getRoute4()
    ifcfg4.value = a
    route4.value = b
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

async function addRoute() {
  if (!rtDst.value.trim()) {
    ElMessage.warning('请输入目的前缀')
    return
  }
  loading.value = true
  try {
    await addRoute4(rtDst.value, rtNh.value, rtPort.value)
    ElMessage.success('已新增')
    rtDst.value = ''
    rtNh.value = ''
    await refresh()
  } catch (e: any) {
    ElMessage.error(e?.message || '新增失败')
  } finally {
    loading.value = false
  }
}

async function deleteRoute(index: number) {
  try {
    await ElMessageBox.confirm('确认删除该路由？', '提示', { type: 'warning' })
    loading.value = true
    await deleteRoute4(index)
    ElMessage.success('已删除')
    await refresh()
  } catch {}
  finally { loading.value = false }
}

async function clearRoutes() {
  try {
    await ElMessageBox.confirm('确认清空所有 IPv4 路由？', '警告', { type: 'warning' })
    loading.value = true
    await clearRoute4()
    ElMessage.success('已清空')
    await refresh()
  } catch {}
  finally { loading.value = false }
}

onMounted(() => refresh())
</script>

<style scoped>
.r4-layout { display:flex; gap:16px; align-items:flex-start; flex-wrap: wrap; }
.r4-left { flex:1; min-width: 520px; }
.r4-right { flex:1; min-width: 520px; }
.r4-head { display:flex; justify-content:space-between; align-items:center; gap: 16px; }
.r4-title { font-weight: 600; font-size: 16px; }
.r4-meta { margin-top: 10px; margin-bottom: 10px; color:#909399; font-size: 12px; }
.r4-form { margin-top: 14px; }
@media (max-width: 1100px) {
  .r4-left, .r4-right { min-width: 0; width: 100%; }
}
</style>
