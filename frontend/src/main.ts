import { createApp } from 'vue'
import { createPinia } from 'pinia'
import App from './App.vue'
import router from './router'
import './assets/main.css'

try {
  const app = createApp(App)
  app.use(createPinia())
  app.use(router)

  app.config.errorHandler = (err, _instance, info) => {
    console.error('Vue error:', err, info)
    const el = document.getElementById('app')
    if (el) {
      el.innerHTML = `<pre style="color:red;padding:2rem">Vue error (${info}):\n${err}\n${(err as Error)?.stack || ''}</pre>`
    }
  }

  router.isReady().then(() => {
    app.mount('#app')
  }).catch((err) => {
    console.error('Router ready error:', err)
    const el = document.getElementById('app')
    if (el) {
      el.innerHTML = `<pre style="color:red;padding:2rem">Router error:\n${err}\n${(err as Error)?.stack || ''}</pre>`
    }
  })
} catch (err) {
  console.error('Init error:', err)
  const el = document.getElementById('app')
  if (el) {
    el.innerHTML = `<pre style="color:red;padding:2rem">Init error:\n${err}\n${(err as Error)?.stack || ''}</pre>`
  }
}
