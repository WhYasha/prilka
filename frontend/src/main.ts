import './assets/main.css'

const el = document.getElementById('app')

function showStatus(msg: string, color = 'green') {
  if (el) el.innerHTML = `<pre style="color:${color};padding:2rem">${msg}</pre>`
  // Beacon for server-side tracking
  navigator.sendBeacon?.(`/api/health?diag=${encodeURIComponent(msg.slice(0, 60))}`)
}

showStatus('1-module-loaded')

async function init() {
  try {
    const { createApp } = await import('vue')
    showStatus('2-vue-imported')

    const { createPinia } = await import('pinia')
    showStatus('3-pinia-imported')

    const { default: App } = await import('./App.vue')
    showStatus('4-app-imported')

    const { default: router } = await import('./router')
    showStatus('5-router-imported')

    const app = createApp(App)
    app.use(createPinia())
    app.use(router)

    app.config.errorHandler = (err, _instance, info) => {
      console.error('Vue error:', err, info)
      showStatus(`Vue-ERROR: ${info}: ${err}`, 'red')
    }

    showStatus('6-mounting')
    app.mount('#app')
    showStatus('7-mounted')
  } catch (err) {
    console.error('Init error:', err)
    showStatus(`INIT-ERROR: ${err}`, 'red')
  }
}

init()
