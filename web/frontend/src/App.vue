<template>
  <el-container class="app-shell">
    <el-aside :width="collapsed ? '64px' : '220px'" class="app-aside">
      <div class="app-logo">
        <div class="app-logo__mark">DP</div>
        <div v-if="!collapsed" class="app-logo__text">Packet Firewall</div>
      </div>
      <el-menu :collapse="collapsed" router :default-active="route.path" class="app-menu">
        <el-menu-item index="/">Dashboard</el-menu-item>
        <el-menu-item index="/acl">ACL规则</el-menu-item>
        <el-menu-item index="/acl6">IPv6 ACL规则</el-menu-item>
        <el-menu-item index="/route6">IPv6 路由/接口</el-menu-item>
        <el-menu-item index="/ddos">抗DDoS/限速</el-menu-item>
        <el-menu-item index="/sessions">会话表</el-menu-item>
        <el-menu-item index="/sessions6">IPv6 会话表</el-menu-item>
      </el-menu>
    </el-aside>
    <el-container>
      <el-header class="app-header">
        <div class="app-header__left">
          <el-button text @click="toggleCollapsed">{{ collapsed ? '展开' : '收起' }}</el-button>
          <div class="app-title">DPDK Packet Firewall</div>
        </div>
        <div class="app-header__right">
          <el-tag :type="healthy ? 'success' : 'danger'" effect="dark">
            {{ healthy ? '控制面正常' : '控制面异常' }}
          </el-tag>
        </div>
      </el-header>
      <el-main class="app-main">
        <router-view />
      </el-main>
    </el-container>
  </el-container>
</template>

<script lang="ts" setup>
import { ref, onMounted, onUnmounted } from 'vue'
import { useRoute } from 'vue-router'
import { getHealth } from './services/api'
const route = useRoute()
const healthy = ref(false)
const collapsed = ref(false)
let timer: any
function toggleCollapsed() {
  collapsed.value = !collapsed.value
}
function applyResponsive() {
  collapsed.value = window.innerWidth < 900
}
async function ping() {
  healthy.value = await getHealth()
}
onMounted(() => {
  applyResponsive()
  window.addEventListener('resize', applyResponsive)
  ping()
  timer = setInterval(ping, 5000)
})
onUnmounted(() => {
  window.removeEventListener('resize', applyResponsive)
  if (timer) clearInterval(timer)
})
</script>

<style>
html, body, #app { height: 100%; margin: 0; }
.app-shell { height: 100vh; background: #f5f7fa; }
.app-aside { border-right: 1px solid #ebeef5; background: #ffffff; }
.app-menu { border-right: none; }
.app-logo { height: 56px; display:flex; align-items:center; gap:10px; padding: 0 14px; border-bottom: 1px solid #ebeef5; }
.app-logo__mark { width: 32px; height: 32px; border-radius: 8px; background: #409eff; color: #fff; display:flex; align-items:center; justify-content:center; font-weight: 700; }
.app-logo__text { font-weight: 600; color: #303133; }
.app-header { height: 56px; display:flex; align-items:center; justify-content:space-between; border-bottom: 1px solid #ebeef5; background: #ffffff; }
.app-header__left { display:flex; align-items:center; gap:10px; }
.app-title { font-weight: 600; color: #303133; }
.app-main { padding: 16px; }
</style>
