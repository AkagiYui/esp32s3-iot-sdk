import type { Plugin } from 'vite'
import { resolve } from 'node:path'
import { readdirSync, readFileSync, writeFileSync } from 'node:fs'
import zlib from 'node:zlib'

export interface PrecompressOptions {
  /** Glob patterns for files to compress (e.g. ['**\/*.html', '**\/*.js']) */
  include?: string[]
  /** Enable gzip (.gz) compression */
  gzip?: boolean
  /** Enable brotli (.br) compression */
  brotli?: boolean
  /** Enable zstandard (.zst) compression */
  zstd?: boolean
  /**
   * Compression ratio threshold (0–1).
   * Compressed file is discarded if compressedSize / originalSize > threshold.
   * @default 0.8
   */
  threshold?: number
  /**
   * Minimum original file size (bytes) to be eligible for compression.
   * @default 1024
   */
  minSize?: number
}

/* ------------------------------------------------------------------ */
/*  Minimal glob matcher (supports *, **, ?)                          */
/* ------------------------------------------------------------------ */

function matchGlob(filePath: string, pattern: string): boolean {
  let re = '^'
  for (let i = 0; i < pattern.length;) {
    const c = pattern[i]
    if (c === '*' && pattern[i + 1] === '*') {
      // **/ → zero or more directory segments; trailing ** → rest of path
      re += pattern[i + 2] === '/' ? '(?:.*/)?' : '.*'
      i += pattern[i + 2] === '/' ? 3 : 2
    } else if (c === '*') {
      re += '[^/]*'
      i++
    } else if (c === '?') {
      re += '[^/]'
      i++
    } else {
      re += c.replace(/[.*+?^${}()|[\]\\]/g, '\\$&')
      i++
    }
  }
  return new RegExp(re + '$').test(filePath)
}

/* ------------------------------------------------------------------ */
/*  Helpers                                                           */
/* ------------------------------------------------------------------ */

function formatSize(bytes: number): string {
  if (bytes < 1024) return `${bytes} B`
  const kb = bytes / 1024
  return kb < 1024 ? `${kb.toFixed(2)} KB` : `${(kb / 1024).toFixed(2)} MB`
}

function walkDir(dir: string, base = ''): string[] {
  const out: string[] = []
  for (const entry of readdirSync(dir, { withFileTypes: true })) {
    const rel = base ? `${base}/${entry.name}` : entry.name
    if (entry.isDirectory()) out.push(...walkDir(resolve(dir, entry.name), rel))
    else out.push(rel)
  }
  return out
}

/* ------------------------------------------------------------------ */
/*  Compression wrappers                                              */
/* ------------------------------------------------------------------ */

function compressGzip(buf: Buffer): Buffer {
  return zlib.gzipSync(buf, { level: 9 })
}

function compressBrotli(buf: Buffer): Buffer {
  return zlib.brotliCompressSync(buf, {
    params: { [zlib.constants.BROTLI_PARAM_QUALITY]: zlib.constants.BROTLI_MAX_QUALITY },
  })
}

const _zstdCompressSync: ((buf: Buffer, opts?: object) => Buffer) | undefined =
  typeof (zlib as any).zstdCompressSync === 'function'
    ? (zlib as any).zstdCompressSync.bind(zlib)
    : undefined

function compressZstd(buf: Buffer): Buffer | undefined {
  return _zstdCompressSync?.(buf, {
    params: { [(zlib.constants as any).ZSTD_c_compressionLevel]: 19 },
  })
}

/* ------------------------------------------------------------------ */
/*  Plugin                                                            */
/* ------------------------------------------------------------------ */

export function precompress(options: PrecompressOptions = {}): Plugin {
  const {
    include = ['**/*.html', '**/*.js', '**/*.css', '**/*.json', '**/*.svg', '**/*.xml', '**/*.txt', '**/*.wasm'],
    gzip: enableGzip = true,
    brotli: enableBrotli = true,
    zstd: enableZstd = true,
    threshold = 0.8,
    minSize = 1024,
  } = options

  let outDir: string

  return {
    name: 'vite-plugin-precompress',
    apply: 'build',

    configResolved(config) {
      outDir = resolve(config.root, config.build.outDir)
    },

    closeBundle() {
      let files: string[]
      try {
        files = walkDir(outDir)
      } catch {
        return
      }

      if (enableZstd && !_zstdCompressSync) {
        console.warn('⚠️  zlib.zstdCompressSync unavailable – skipping zstd compression')
      }

      const algorithms: Array<{
        key: string
        ext: string
        enabled: boolean
        compress: (buf: Buffer) => Buffer | undefined
      }> = [
          { key: 'gz', ext: '.gz', enabled: enableGzip, compress: compressGzip },
          { key: 'br', ext: '.br', enabled: enableBrotli, compress: compressBrotli },
          { key: 'zst', ext: '.zst', enabled: enableZstd && !!_zstdCompressSync, compress: compressZstd },
        ]

      type Row = { file: string; originalSize: number;[k: string]: any }
      const results: Row[] = []

      for (const file of files) {
        if (!include.some(p => matchGlob(file, p))) continue

        const fullPath = resolve(outDir, file)
        const content = readFileSync(fullPath)
        if (content.length < minSize) continue

        const row: Row = { file, originalSize: content.length }

        for (const algo of algorithms) {
          if (!algo.enabled) continue
          const compressed = algo.compress(content)
          if (!compressed) continue
          const ratio = compressed.length / content.length
          if (ratio <= threshold) {
            writeFileSync(`${fullPath}${algo.ext}`, compressed)
            row[algo.key] = compressed.length
          }
        }

        results.push(row)
      }

      // ── Pretty print ──────────────────────────────────────────
      if (results.length === 0) return

      console.log('\n🗜️  Pre-compression results:')
      for (const r of results) {
        const parts: string[] = []
        for (const algo of algorithms) {
          if (r[algo.key] != null) {
            const pct = ((r[algo.key] as number) / r.originalSize * 100).toFixed(1)
            parts.push(`${algo.key}: ${formatSize(r[algo.key] as number)} (${pct}%)`)
          }
        }
        console.log(
          `  ${r.file}  ${formatSize(r.originalSize)} → ${parts.join(' | ') || 'all skipped (below threshold)'}`,
        )
      }
      console.log()
    },
  }
}
