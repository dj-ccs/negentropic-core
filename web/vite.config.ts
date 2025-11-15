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

          // Also add COEP/COOP headers to ensure consistency
          res.setHeader('Cross-Origin-Embedder-Policy', 'require-corp');
          res.setHeader('Cross-Origin-Opener-Policy', 'same-origin');

          // Allow credentials for same-origin requests
          res.setHeader('Access-Control-Allow-Credentials', 'true');

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
      'Cross-Origin-Resource-Policy': 'cross-origin',
    },
    fs: {
      // Allow serving files from parent directory (for WASM build)
      allow: ['..'],
    },
    // Proxy OSM tiles to bypass COEP blocking
    proxy: {
      '/osm-tiles': {
        target: 'https://a.tile.openstreetmap.org',
        changeOrigin: true,
        rewrite: (path) => path.replace(/^\/osm-tiles/, ''),
        configure: (proxy, _options) => {
          proxy.on('proxyRes', (proxyRes, _req, _res) => {
            // Add CORP header to OSM tiles so COEP doesn't block them
            proxyRes.headers['cross-origin-resource-policy'] = 'cross-origin';
          });
        },
      },
    },
  },
  preview: {
    port: 3000,
    headers: {
      'Cross-Origin-Embedder-Policy': 'require-corp',
      'Cross-Origin-Opener-Policy': 'same-origin',
      'Cross-Origin-Resource-Policy': 'cross-origin',
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
