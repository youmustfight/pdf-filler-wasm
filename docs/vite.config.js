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
      copyFileSync(src, resolve(__dirname, 'build/assets/pdf-filler.wasm'));
    }
  }]
});
