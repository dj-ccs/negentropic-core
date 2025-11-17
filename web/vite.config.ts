import { defineConfig } from 'vite';
import { viteStaticCopy } from 'vite-plugin-static-copy';
import wasm from 'vite-plugin-wasm';
import { resolve } from 'path';

export default defineConfig({
  plugins: [
    wasm(),
    // This plugin copies Cesium's static assets to the public folder
    // so they are served from the same origin, satisfying COEP.
    viteStaticCopy({
      targets: [
        {
          src: 'node_modules/cesium/Build/Cesium/Workers',
          dest: 'cesium'
        },
        {
          src: 'node_modules/cesium/Build/Cesium/ThirdParty',
          dest: 'cesium'
        },
        {
          src: 'node_modules/cesium/Build/Cesium/Assets',
          dest: 'cesium'
        },
        {
          src: 'node_modules/cesium/Build/Cesium/Widgets',
          dest: 'cesium'
        }
      ]
    }),
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
    // Proxy API calls to make them appear as same-origin to the browser
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
      // Proxy Cesium Ion API calls to appear as same-origin, preventing COEP blocking.
      // This allows IonImageryProvider to load successfully under strict COEP requirements.
      '/cesium-ion-api': {
        target: 'https://api.cesium.com/',
        changeOrigin: true,
        rewrite: (path) => path.replace(/^\/cesium-ion-api/, ''),
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
          // transformers: ['@xenova/transformers'], // Temporarily disabled
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
