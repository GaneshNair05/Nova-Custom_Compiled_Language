#!/usr/bin/env node
// Stands in for mycompiler.exe during local testing of runNova.js's
// subprocess handling (spawn, timeout, output capture) — mimics the real
// binary's exact stdout shape from main.cpp, and a couple of special-cased
// source strings to exercise the timeout and stderr paths.
const fs = require('fs');

const srcPath = process.argv[2];
const source = fs.readFileSync(srcPath, 'utf8');

console.log('identifier : announce');
console.log('LeftParen : (');
console.log('Number : 1');
console.log('RightParen : )');
console.log('EndOfFile : ');
console.log('');
console.log('--- Generating LLVM IR ---');
console.log('define double @main() {');
console.log('entry:');
console.log('  ret double 0.000000e+00');
console.log('}');
console.log('');
console.log('--- Executing Program ---');

if (source.includes('__STUB_HANG__')) {
  // never exits — exercises runNova.js's SIGKILL timeout path
  setInterval(() => {}, 1000);
} else if (source.includes('__STUB_STDERR__')) {
  console.error('Runtime Error: something went wrong');
  process.exit(1);
} else {
  console.log('>>> 1.000000');
}
