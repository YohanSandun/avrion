import { defineConfig } from 'vite';
import react from '@vitejs/plugin-react';

export default defineConfig({
  plugins: [react()],
  base: '/avrion/',
  assetsInclude: ['**/*.wasm'],
  server: {
    headers: {
      // Required for SharedArrayBuffer (used by some Emscripten builds)
      'Cross-Origin-Opener-Policy': 'same-origin',
      'Cross-Origin-Embedder-Policy': 'require-corp',
    },
  },
});
