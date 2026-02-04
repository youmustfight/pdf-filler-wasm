import { defineConfig } from 'vite';
import { resolve } from 'path';
import { copyFileSync } from 'fs';

export default defineConfig({
  root: 'src',
  base: './',
  build: {
    outDir: resolve(__dirname, 'build'),
    emptyOutDir: true,
    target: 'esnext'
  },
  plugins: [{
    name: 'copy-wasm',
    closeBundle() {
      const src = resolve(__dirname, 'node_modules/pdf-filler-wasm/dist/pdf-filler.wasm');
      // Copy to build root - Emscripten looks for WASM relative to the page
      copyFileSync(src, resolve(__dirname, 'build/pdf-filler.wasm'));
    }
  }]
});
