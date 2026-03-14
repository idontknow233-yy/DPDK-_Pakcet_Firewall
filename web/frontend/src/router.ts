import { createRouter, createWebHistory } from 'vue-router'
import AclPage from './pages/AclPage.vue'
import Acl6Page from './pages/Acl6Page.vue'
import DashboardPage from './pages/DashboardPage.vue'
import DdosPage from './pages/DdosPage.vue'
import SessionsPage from './pages/SessionsPage.vue'
import Sessions6Page from './pages/Sessions6Page.vue'
import Route6Page from './pages/Route6Page.vue'

const routes = [
  { path: '/', component: DashboardPage },
  { path: '/acl', component: AclPage },
  { path: '/acl6', component: Acl6Page },
  { path: '/route6', component: Route6Page },
  { path: '/ddos', component: DdosPage },
  { path: '/sessions', component: SessionsPage },
  { path: '/sessions6', component: Sessions6Page }
]

const router = createRouter({
  history: createWebHistory(),
  routes
})

export default router
