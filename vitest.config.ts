import { defineConfig } from 'vitest/config';
import path from 'path';

export default defineConfig({
  test: {
    globals: true,
    environment: 'node',
    include: ['test/**/*.test.ts'],
    testTimeout: 30000,
  },
  resolve: {
    alias: {
      './pdf-filler.js': path.resolve(__dirname, 'dist/pdf-filler.js'),
    },
  },
});
