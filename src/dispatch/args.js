'use strict';
// Argument tokenizing + flag parsing shared by the CLI, dispatcher, and scripts.

// Split a command-line string into argv, respecting single/double quotes.
function splitArgs(str) {
  const out = [];
  const re = /"([^"]*)"|'([^']*)'|(\S+)/g;
  let m;
  while ((m = re.exec(str)) !== null) {
    out.push(m[1] !== undefined ? m[1] : m[2] !== undefined ? m[2] : m[3]);
  }
  return out;
}

// Flags that take the following token as their value; everything else --flag is
// boolean true. Keeps `ls --tag group:science` working without ambiguity.
const VALUE_FLAGS = new Set(['tag', 'level', 'tail', 'message', 'm', 'state', 'port', 'as', 'name', 'note', 'mantle']);

function parseArgs(argv) {
  const positional = [];
  const flags = {};
  for (let i = 0; i < argv.length; i++) {
    const a = argv[i];
    if (typeof a === 'string' && a.startsWith('--')) {
      const body = a.slice(2);
      const eq = body.indexOf('=');
      if (eq >= 0) { flags[body.slice(0, eq)] = body.slice(eq + 1); continue; }
      if (VALUE_FLAGS.has(body) && argv[i + 1] !== undefined) { flags[body] = argv[++i]; continue; }
      flags[body] = true;
    } else if (typeof a === 'string' && a.startsWith('-') && a.length === 2) {
      const k = a.slice(1);
      if (VALUE_FLAGS.has(k) && argv[i + 1] !== undefined) { flags[k] = argv[++i]; }
      else flags[k] = true;
    } else {
      positional.push(a);
    }
  }
  return { positional, flags };
}

module.exports = { splitArgs, parseArgs };
