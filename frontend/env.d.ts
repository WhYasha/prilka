/// <reference types="vite/client" />

declare module 'emoji-mart-vue-fast' {
  import type { DefineComponent } from 'vue'
  export const Picker: DefineComponent<Record<string, unknown>, Record<string, unknown>, unknown>
  export const EmojiIndex: any
  export const store: any
  export const frequently: any
}

declare module 'emoji-mart-vue-fast/data/all.json' {
  const data: any
  export default data
}

declare module '*.vue' {
  import type { DefineComponent } from 'vue'
  const component: DefineComponent<Record<string, unknown>, Record<string, unknown>, unknown>
  export default component
}
