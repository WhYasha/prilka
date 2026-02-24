import { createRouter, createWebHistory } from 'vue-router'
import { useAuthStore } from '@/stores/auth'

const router = createRouter({
  history: createWebHistory(),
  routes: [
    {
      path: '/login',
      name: 'login',
      component: () => import('@/views/LoginView.vue'),
      meta: { guest: true },
    },
    {
      path: '/register',
      name: 'register',
      component: () => import('@/views/RegisterView.vue'),
      meta: { guest: true },
    },
    {
      path: '/app',
      name: 'messenger',
      component: () => import('@/views/MessengerView.vue'),
      meta: { auth: true },
    },
    {
      path: '/@:username',
      name: 'userProfile',
      component: () => import('@/views/MessengerView.vue'),
      meta: { auth: true },
    },
    {
      path: '/dm/:id',
      name: 'dm',
      component: () => import('@/views/MessengerView.vue'),
      meta: { auth: true },
    },
    {
      path: '/c/:name',
      name: 'channel',
      component: () => import('@/views/MessengerView.vue'),
      meta: { auth: true },
    },
    {
      path: '/c/:chatId/m/:messageId',
      name: 'messageDeepLink',
      component: () => import('@/views/MessengerView.vue'),
      meta: { auth: true },
    },
    {
      path: '/u/:id',
      name: 'userById',
      component: () => import('@/views/MessengerView.vue'),
      meta: { auth: true },
    },
    {
      path: '/join/:token',
      name: 'join',
      component: () => import('@/views/JoinView.vue'),
    },
    {
      path: '/admin',
      component: () => import('@/views/AdminView.vue'),
      meta: { admin: true },
      children: [
        {
          path: '',
          name: 'adminDashboard',
          component: () => import('@/components/admin/AdminDashboard.vue'),
        },
        {
          path: 'users',
          name: 'adminUsers',
          component: () => import('@/components/admin/AdminUsers.vue'),
        },
        {
          path: 'users/:id',
          name: 'adminUserDetail',
          component: () => import('@/components/admin/AdminUserDetail.vue'),
        },
        {
          path: 'messages',
          name: 'adminMessages',
          component: () => import('@/components/admin/AdminMessages.vue'),
        },
        {
          path: 'support',
          name: 'adminSupport',
          component: () => import('@/components/admin/AdminSupport.vue'),
        },
      ],
    },
    {
      path: '/',
      redirect: '/app',
    },
  ],
})

router.beforeEach(async (to) => {
  const authStore = useAuthStore()

  // Try to fetch user if we have token but no user data
  if (authStore.accessToken && !authStore.user) {
    try {
      await authStore.fetchMe()
    } catch {
      authStore.logout()
      if (to.meta.auth || to.meta.admin) {
        return '/login'
      }
    }
  }

  // Auth-required routes
  if (to.meta.auth && !authStore.isLoggedIn) {
    return '/login'
  }

  // Guest-only routes (redirect logged-in users away)
  if (to.meta.guest && authStore.isLoggedIn) {
    return '/app'
  }

  // Admin routes
  if (to.meta.admin) {
    if (!authStore.isLoggedIn) return '/login'
    if (!authStore.isAdmin) return '/app'
  }
})

export default router
