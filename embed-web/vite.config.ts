import { resolve } from 'path'
import { defineConfig } from 'vite'
import { svelte } from '@sveltejs/vite-plugin-svelte'
import { viteSingleFile } from "vite-plugin-singlefile"
import { precompress } from './plugins/vite-plugin-precompress'
import { viteBundleSize } from './plugins/vite-plugin-bundle-size'

// https://vite.dev/config/
export default defineConfig({
  plugins: [svelte(), viteSingleFile(), precompress(), viteBundleSize()],
  resolve: {
    alias: {
      '@': resolve(__dirname, 'src'),
    },
  },
})
