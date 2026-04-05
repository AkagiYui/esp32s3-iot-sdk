import { resolve } from 'path'
import { readdirSync, statSync } from 'fs'
import { defineConfig, type Plugin } from 'vite'
import { svelte } from '@sveltejs/vite-plugin-svelte'
import { viteSingleFile } from "vite-plugin-singlefile"
import { precompress } from './plugins/vite-plugin-precompress'

function viteBundleSize(): Plugin {
  return {
    name: 'vite-plugin-bundle-size',
    closeBundle() {
      const outDir = resolve(__dirname, 'dist')
      let totalBytes = 0
      function walk(dir: string) {
        for (const entry of readdirSync(dir, { withFileTypes: true })) {
          const fullPath = resolve(dir, entry.name)
          if (entry.isDirectory()) {
            walk(fullPath)
          } else {
            totalBytes += statSync(fullPath).size
          }
        }
      }
      try {
        walk(outDir)
        const kb = (totalBytes / 1024).toFixed(2)
        console.log(`\n📦 Build output size: ${totalBytes} bytes (${kb} KB)\n`)
      } catch {
        // outDir doesn't exist, skip
      }
    },
  }
}

// https://vite.dev/config/
export default defineConfig({
  plugins: [svelte(), viteSingleFile(), precompress(), viteBundleSize()],
  resolve: {
    alias: {
      '@': resolve(__dirname, 'src'),
    },
  },
})
