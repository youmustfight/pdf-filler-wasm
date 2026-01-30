import * as esbuild from 'esbuild';

const sharedConfig = {
  entryPoints: ['src/index.ts'],
  bundle: true,
  platform: 'neutral',
  target: ['es2020'],
  sourcemap: true,
  external: ['./pdf-filler.js'],
};

// ESM build
await esbuild.build({
  ...sharedConfig,
  outfile: 'dist/index.mjs',
  format: 'esm',
});

// CJS build
await esbuild.build({
  ...sharedConfig,
  outfile: 'dist/index.js',
  format: 'cjs',
});

console.log('Build complete');
