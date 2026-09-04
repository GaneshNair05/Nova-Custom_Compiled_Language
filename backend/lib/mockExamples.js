const fs = require('fs');
const path = require('path');

const examples = JSON.parse(
  fs.readFileSync(path.join(__dirname, '..', 'data', 'examples.json'), 'utf8')
);

// Traced by hand against the compiler's actual semantics (the tagged
// [tag][length][elements...] heap layout for arrays/strings, nova_print's
// "%f" formatting, nova_print_array's "%g" formatting, and the bounds-check
// error path) — not a second, possibly-drifting interpreter. Only exact
// matches against the bundled examples get real output; anything else in
// demo mode gets an explanatory message rather than a guess.
const outputs = {
  basics: {
    output: ['13.000000', '7.000000', '30.000000', '3.000000', '1.000000']
      .map((n) => `>>> ${n}`)
      .join('\n'),
    stderr: '',
  },
  'control-flow': {
    output: ['3.000000', '6.000000', '9.000000', '12.000000', '15.000000']
      .map((n) => `>>> ${n}`)
      .join('\n'),
    stderr: '',
  },
  arrays: {
    output: [
      '>>> 5.000000',
      '>>> [10, 20, 30, 0, 0]',
      '>>> 20.000000',
      '>>> 0.000000',
    ].join('\n'),
    stderr: 'Runtime Error: array index 10 out of bounds (length 5)',
  },
  strings: {
    output: [
      '>>> Hello, World!',
      '>>> 13.000000',
      '>>> 1.000000',
      '>>> 0.000000',
    ].join('\n'),
    stderr: '',
  },
};

function findMatchingExample(code) {
  const normalized = code.trim();
  return examples.find((ex) => ex.code.trim() === normalized) || null;
}

function getMockResult(code) {
  const match = findMatchingExample(code);
  if (!match || !outputs[match.id]) {
    return {
      mode: 'demo',
      demoUnmatched: true,
      tokens: '',
      ir: '',
      output: '',
      stderr: '',
      error:
        'Demo mode: no compiler is configured (NOVA_COMPILER_PATH unset), so only the ' +
        'bundled examples, run unmodified, produce output here. Pick one from the dropdown, ' +
        'or set NOVA_COMPILER_PATH to run anything you write — see README.md.',
    };
  }
  return {
    mode: 'demo',
    demoUnmatched: false,
    tokens: '',
    ir: '',
    ...outputs[match.id],
  };
}

module.exports = { getMockResult, findMatchingExample };
