import { defineConfig } from 'vite';
import cesium from 'vite-plugin-cesium';
import wasm from 'vite-plugin-wasm';
import { resolve } from 'path';

export default defineConfig({
  plugins: [
    cesium(),
    wasm(),
  ],
  server: {
    port: 3000,
    headers: {
      // Required for SharedArrayBuffer support
      'Cross-Origin-Embedder-Policy': 'require-corp',
      'Cross-Origin-Opener-Policy': 'same-origin',
    },
    fs: {
      // Allow serving files from parent directory (for WASM build)
      allow: ['..'],
    },
  },
  preview: {
    port: 3000,
    headers: {
      'Cross-Origin-Embedder-Policy': 'require-corp',
      'Cross-Origin-Opener-Policy': 'same-origin',
    },
  },
  resolve: {
    alias: {
      '@': resolve(__dirname, './src'),
    },
  },
  build: {
    target: 'es2022',
    outDir: 'dist',
    sourcemap: true,
    rollupOptions: {
      output: {
        manualChunks: {
          cesium: ['cesium'],
          deckgl: ['@deck.gl/core', '@deck.gl/layers', '@deck.gl/geo-layers'],
          transformers: ['@xenova/transformers'],
        },
      },
    },
  },
  optimizeDeps: {
    exclude: ['@xenova/transformers'],
    esbuildOptions: {
      target: 'es2022',
    },
  },
  worker: {
    format: 'es',
    plugins: [wasm()],
  },
});
