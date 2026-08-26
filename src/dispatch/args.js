'use strict';
// Argument tokenizing + flag parsing shared by the CLI, dispatcher, and scripts.

// Split a command-line string into argv per SPEC §6.1 — a character scanner
// mirroring core/src/dispatch/args.c, NOT a regex. The regex this replaced diverged
// from the C core on three counts, each of which silently produced a different argv:
// it had no `\'` escape (so 'don\'t' became TWO arguments — the exact truncation
// Void Hormiga reported on 2026-08-17), it was not strip-anywhere (a'b'c kept its
// quotes instead of yielding abc), and it split a trailing backslash off its value.
// Conformance cases 12-arg-quoting.vs and 13-transcript-safety.vs pin all of it.

// Quote an arbitrary value as ONE dispatcher argument (SPEC §6.1), mirroring
// vc_arg_quote in core/src/dispatch/args.c and quote_arg in the Python binding.
// The law, pinned by voidcore/codec_test.py as a property over generated inputs:
//     splitArgs(quoteArg(v)) deep-equals [v]   for every v
// Trailing backslashes are emitted OUTSIDE the closing quote. Without that, a
// value ending in a backslash puts one immediately before the closing quote,
// rule 3 reads the pair as an escaped apostrophe, and the argument never closes.
function quoteArg(value) {
  const v = String(value == null ? '' : value);
  let head = v.length;
  while (head > 0 && v[head - 1] === '\\') head--;
  return "'" + v.slice(0, head).replace(/'/g, "\\'") + "'" + v.slice(head);
}

// Thrown when a quoted run is still open at end of input (SPEC §6.1 rule 5).
// Since 0.2.7 that is an error rather than a silent run-to-end-of-input: the
// silence is what made every bug in this class content corruption with ok:true.
class UnterminatedQuote extends Error {
  constructor(message) { super(message || 'unterminated quote (SPEC §6.1 rule 5)'); this.name = 'UnterminatedQuote'; }
}

function splitArgs(str) {
  const out = [];
  const s = String(str);
  let i = 0;
  while (i < s.length) {
    while (i < s.length && /\s/.test(s[i])) i++;      // skip separators
    if (i >= s.length) break;
    let buf = '';
    let quote = 0;                                     // 0 = outside quotes
    while (i < s.length) {
      const c = s[i];
      if (quote) {
        // inside SINGLE quotes, \' is a literal quote; every other backslash is
        // literal (so JSON payloads and text codes like \cY pass through intact)
        if (quote === "'" && c === '\\' && s[i + 1] === "'") { buf += "'"; i += 2; continue; }
        if (c === quote) { quote = 0; i++; continue; } // closing quote is stripped
      } else {
        if (c === "'" || c === '"') { quote = c; i++; continue; }  // opening, stripped
        if (/\s/.test(c)) break;
      }
      buf += c;
      i++;
    }
    if (quote) throw new UnterminatedQuote();         // rule 5 (0.2.7): loud, not silent
    out.push(buf);
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

module.exports = { splitArgs, parseArgs, quoteArg, UnterminatedQuote };
