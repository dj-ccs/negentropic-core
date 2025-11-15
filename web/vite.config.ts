import { defineConfig } from 'vite';
import cesium from 'vite-plugin-cesium';
import wasm from 'vite-plugin-wasm';
import { resolve } from 'path';

export default defineConfig({
  plugins: [
    cesium(),
    wasm(),
    {
      name: 'configure-response-headers',
      configureServer: (server) => {
        server.middlewares.use((_req, res, next) => {
          // Add CORP header to all resources to satisfy COEP require-corp
          res.setHeader('Cross-Origin-Resource-Policy', 'cross-origin');
          next();
        });
      },
    },
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
          // cesium removed - it's external via vite-plugin-cesium
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
    plugins: () => [wasm()],
  },
});
