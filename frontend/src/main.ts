import './assets/main.css'

const el = document.getElementById('app')

function showStatus(msg: string, color = 'green') {
  if (el) el.innerHTML = `<pre style="color:${color};padding:2rem">${msg}</pre>`
}

showStatus('JS module loaded. Importing Vue...')

async function init() {
  try {
    const { createApp } = await import('vue')
    showStatus('Vue imported. Importing Pinia...')

    const { createPinia } = await import('pinia')
    showStatus('Pinia imported. Importing App...')

    const { default: App } = await import('./App.vue')
    showStatus('App imported. Importing Router...')

    const { default: router } = await import('./router')
    showStatus('All imports OK. Mounting app...')

    const app = createApp(App)
    app.use(createPinia())
    app.use(router)

    app.config.errorHandler = (err, _instance, info) => {
      console.error('Vue error:', err, info)
      showStatus(`Vue error (${info}):\n${err}\n${(err as Error)?.stack || ''}`, 'red')
    }

    app.mount('#app')
  } catch (err) {
    console.error('Init error:', err)
    showStatus(`Init error:\n${err}\n${(err as Error)?.stack || ''}`, 'red')
  }
}

init()
