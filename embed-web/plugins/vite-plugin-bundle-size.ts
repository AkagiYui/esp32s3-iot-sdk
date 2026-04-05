import type { Plugin } from 'vite'
import { resolve } from 'node:path'
import { readdirSync, statSync } from 'node:fs'

export function viteBundleSize(): Plugin {
  let outDir: string

  return {
    name: 'vite-plugin-bundle-size',
    configResolved(config) {
      outDir = resolve(config.root, config.build.outDir)
    },
    closeBundle() {
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
