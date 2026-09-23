import { createRouter, createWebHistory } from 'vue-router'
import { useAuthStore } from '@/stores/auth'

const router = createRouter({
  history: createWebHistory(),
  routes: [
    {
      path: '/login',
      name: 'login',
      component: () => import('@/views/LoginView.vue'),
      meta: { publica: true },
    },
    { path: '/', name: 'produccion', component: () => import('@/views/ProduccionView.vue') },
    { path: '/historico', name: 'historico', component: () => import('@/views/HistoricoView.vue') },
    { path: '/intentos', name: 'intentos', component: () => import('@/views/IntentosView.vue') },
    { path: '/alertas', name: 'alertas', component: () => import('@/views/AlertasView.vue') },
  ],
})

router.beforeEach((to) => {
  const auth = useAuthStore()
  if (!to.meta.publica && !auth.token) return { name: 'login', query: { next: to.fullPath } }
  if (to.name === 'login' && auth.token) return { name: 'produccion' }
})

export default router
