import { defineConfig } from 'vite';
import { resolve } from 'path';
import { copyFileSync, mkdirSync } from 'fs';

export default defineConfig({
  root: 'src',
  base: '/pdf-filler-js/',
  build: {
    outDir: resolve(__dirname, 'build'),
    emptyOutDir: true,
    target: 'esnext'
  },
  plugins: [{
    name: 'copy-wasm',
    closeBundle() {
      const src = resolve(__dirname, 'node_modules/pdf-filler-wasm/dist/pdf-filler.wasm');
      const destDir = resolve(__dirname, 'build/assets');
      const dest = resolve(destDir, 'pdf-filler.wasm');
      mkdirSync(destDir, { recursive: true });
      copyFileSync(src, dest);
    }
  }]
});
