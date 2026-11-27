#!/usr/bin/env node
//
// Run the Sedona VM compiled to WebAssembly under Node.
//
//   node adm/wasm/runwasm.mjs <svm.mjs> <scode> -test        run the test suite
//   node adm/wasm/runwasm.mjs <svm.mjs> <scode> <app.sab> [--cycles n]
//                                                            run an app (Ctrl-C to stop,
//                                                            or after n scan cycles)
//
// Exits with the number of failed asserts (tests) or the VM's error code.
//
import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';
import { pathToFileURL } from 'node:url';

const ERR_YIELD = 253;
const ERR_HIBERNATE = 255;

const [svmPath, scodePath, arg, ...opts] = process.argv.slice(2);
const cyclesAt = opts.indexOf('--cycles');
const maxCycles = cyclesAt >= 0 ? Number(opts[cyclesAt + 1]) : Number.POSITIVE_INFINITY;
if (!svmPath || !scodePath || !arg) {
  console.error('usage: runwasm.mjs <svm.mjs> <scode> (-test | <app.sab>)');
  process.exit(2);
}

const { default: createSvm } = await import(pathToFileURL(resolve(svmPath)).href);
const svm = await createSvm();
svm.FS.writeFile('/kits.scode', readFileSync(scodePath));
let vmArg = arg;
if (arg !== '-test' && !arg.startsWith('-')) {
  svm.FS.writeFile('/app.sab', readFileSync(arg));
  vmArg = '/app.sab';
}

const start = svm.cwrap('svm_start', 'number', ['string', 'string']);
const resume = svm.cwrap('svm_resume', 'number', []);
const yieldNs = svm.cwrap('svm_yield_ns', 'number', []);

let r = start('/kits.scode', vmArg);
let cycles = r === ERR_YIELD ? 1 : 0;
while ((r === ERR_YIELD || r === ERR_HIBERNATE) && cycles < maxCycles) {
  const ms = Math.max(0, yieldNs() / 1e6);
  await new Promise((done) => setTimeout(done, ms));
  r = resume();
  if (r === ERR_YIELD) cycles++;
}
if (cycles >= maxCycles) {
  console.log(`wasm: app ran ${cycles} scan cycles`);
  process.exit(0);
}

if (arg === '-test') {
  const failures = svm._svm_assert_failures();
  console.log(`wasm: ${svm._svm_assert_successes()} asserts passed, ${failures} failed, exit ${r}`);
  process.exit(failures > 0 ? Math.min(failures, 125) : r);
}
process.exit(r);
