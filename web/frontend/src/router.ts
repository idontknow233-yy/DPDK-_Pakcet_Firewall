import { createRouter, createWebHistory } from 'vue-router'
import AclPage from './pages/AclPage.vue'
import DashboardPage from './pages/DashboardPage.vue'
import SessionsPage from './pages/SessionsPage.vue'

const routes = [
  { path: '/', component: DashboardPage },
  { path: '/acl', component: AclPage },
  { path: '/sessions', component: SessionsPage }
]

const router = createRouter({
  history: createWebHistory(),
  routes
})

export default router
