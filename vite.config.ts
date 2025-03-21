import { defineConfig } from 'vite'
import react from '@vitejs/plugin-react'

export default defineConfig({
  plugins: [ react() ],
  root: '.',                // your project root
  publicDir: 'public',      // where wasm assets live
})