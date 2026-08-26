'use strict';
// Voidscript — the terminal-complete scripting language (ARCHITECTURE §5).
// An interpreter over the dispatcher: every command line is a Void Core command,
// wrapped in control flow (let / if / foreach / while / repeat / def / try-catch /
// on error / include / call / halt / return / wait / assert / echo).
//
// runScript(src, { dispatch, logger, args }) -> { ok, lines, data }

const { quoteArg } = require('../dispatch/args.js');

// ── Signals ─────────────────────────────────────────────────────────
class Halt { constructor(code) { this.code = code || 0; } }
class Return { constructor(value) { this.value = value; } }
class Break {}
class Continue {}

// ── Tokenizer ───────────────────────────────────────────────────────
// Quoting is SPEC §6.1's, and it has to be, because a WORD here becomes a
// dispatcher argument. Before 0.2.7 this scanner had its own rules — no `\'`
// escape, quotes as delimiters rather than strip-anywhere, and an unterminated
// quote running silently to end of input — which is the same divergence the C
// core carried in three separate places. The consequence is not cosmetic: a
// value written the way §6.1 tells hosts to write it (`'don\'t …'`) closed its
// quoted run early, and whatever followed the next newline became a statement.
//
// `lit` is a per-character mask of the token's value: '1' where the character
// came from inside a SINGLE-quoted run. Single quotes suppress `$` expansion
// (SPEC §8) — without that, a transcript built by correctly quoting somebody
// else's text runs whatever `$(...)` that text contains.
function tokenize(src) {
  const toks = [];
  let i = 0;
  const n = src.length;
  const push = (type, value, quoted = false, lit = '') => toks.push({ type, value, quoted, lit });
  while (i < n) {
    const ch = src[i];
    if (ch === '#') { while (i < n && src[i] !== '\n') i++; continue; }
    if (ch === '\n' || ch === ';') { push('NL'); i++; continue; }
    if (ch === ' ' || ch === '\t' || ch === '\r') { i++; continue; }
    if (ch === '{' || ch === '}' || ch === '(' || ch === ')' || ch === ',') { push(ch); i++; continue; }
    // one WORD: runs until an unquoted separator, crossing quote boundaries
    // (rule 2 is strip-anywhere, so a'b'c is the single argument abc)
    let s = '';
    let lit = '';
    let quote = 0;
    let sawQuote = false;
    while (i < n) {
      const c = src[i];
      if (quote) {
        // rule 3: inside single quotes the ONLY escape is \'
        if (quote === "'" && c === '\\' && src[i + 1] === "'") { s += "'"; lit += '1'; i += 2; continue; }
        if (c === quote) { quote = 0; i++; continue; }      // closing quote, stripped
        s += c; lit += quote === "'" ? '1' : '0'; i++; continue;
      }
      if (c === '"' || c === "'") { quote = c; sawQuote = true; i++; continue; } // opening, stripped
      if (' \t\r\n;{}(),'.includes(c) || c === '#') break;
      s += c; lit += '0'; i++;
    }
    // rule 5 (0.2.7): a quoted run still open at end of input is an ERROR. It
    // used to run to end of input, which is precisely what made this whole class
    // of bug silent — the value swallowed the rest of the file, ok:true.
    if (quote) throw new Error('Voidscript: unterminated quote (SPEC §6.1 rule 5)');
    push('WORD', s, sawQuote, lit);
  }
  push('EOF');
  return toks;
}

// ── Parser → statement list ─────────────────────────────────────────
function parse(toks) {
  let p = 0;
  const peek = () => toks[p];
  const at = (t) => toks[p] && toks[p].type === t;
  const eat = () => toks[p++];
  const skipNL = () => { while (at('NL')) p++; };
  const expect = (t) => { if (!at(t)) throw new Error(`Voidscript: expected ${t}, got ${toks[p].type} "${toks[p].value || ''}"`); return eat(); };

  // collect WORD/( ) tokens until one of the stop types (NL or '{')
  function collectUntil(...stops) {
    const out = [];
    while (peek() && !stops.includes(peek().type) && peek().type !== 'EOF') out.push(eat());
    return out;
  }
  // collect a statement's arguments: stops at newline OR a closing brace, so
  // inline blocks like `try { rune new x } catch (e) { echo y }` parse correctly.
  const collectArgs = () => collectUntil('NL', '}');

  function parseBlock() {
    expect('{'); skipNL();
    const stmts = [];
    while (!at('}') && !at('EOF')) { stmts.push(parseStatement()); skipNL(); }
    expect('}');
    return stmts;
  }

  function parseStatement() {
    skipNL();
    const t = peek();
    if (t.type === 'WORD' && !t.quoted) {
      const kw = t.value;
      if (kw === 'let') {
        eat(); const name = expect('WORD').value; expect('WORD'); // '='
        const expr = collectArgs(); return { kind: 'let', name, expr };
      }
      if (kw === 'echo' || kw === 'print') { eat(); return { kind: 'echo', expr: collectArgs() }; }
      if (kw === 'if') return parseIf();
      if (kw === 'while') { eat(); const cond = collectUntil('{'); return { kind: 'while', cond, body: parseBlock() }; }
      if (kw === 'repeat') { eat(); const count = collectUntil('{'); return { kind: 'repeat', count, body: parseBlock() }; }
      if (kw === 'foreach') {
        eat(); const v = expect('WORD').value; expect('WORD'); // 'in'
        expect('('); const cmd = collectUntil(')'); expect(')');
        return { kind: 'foreach', var: v, cmd, body: parseBlock() };
      }
      if (kw === 'def') {
        eat(); const name = expect('WORD').value; expect('(');
        const params = [];
        while (!at(')')) { if (at('WORD')) params.push(eat().value); else eat(); }
        expect(')'); return { kind: 'def', name, params, body: parseBlock() };
      }
      if (kw === 'try') {
        eat(); const body = parseBlock(); skipNL();
        let cv = 'e', handler = [];
        if (at('WORD') && peek().value === 'catch') { eat(); expect('('); cv = expect('WORD').value; expect(')'); handler = parseBlock(); }
        return { kind: 'try', body, catchVar: cv, handler };
      }
      if (kw === 'on') { eat(); expect('WORD'); /* error */ const mode = expect('WORD').value; return { kind: 'onerror', mode }; }
      if (kw === 'halt' || kw === 'return') { eat(); return { kind: kw, expr: collectArgs() }; }
      if (kw === 'break') { eat(); return { kind: 'break' }; }
      if (kw === 'continue') { eat(); return { kind: 'continue' }; }
      if (kw === 'wait') { eat(); return { kind: 'wait', expr: collectArgs() }; }
      if (kw === 'include') { eat(); return { kind: 'include', expr: collectArgs() }; }
      if (kw === 'call') { eat(); return { kind: 'call', expr: collectArgs() }; }
      if (kw === 'assert') { eat(); return { kind: 'assert', cmd: collectArgs() }; }
      // function call:  name ( args )
      if (toks[p + 1] && toks[p + 1].type === '(') {
        const name = eat().value; expect('(');
        const args = [];
        let cur = [];
        while (!at(')') && !at('EOF')) { if (at(',')) { args.push(cur); cur = []; eat(); } else cur.push(eat()); }
        if (cur.length) args.push(cur);
        expect(')');
        return { kind: 'fcall', name, args };
      }
    }
    // default: a command statement (tokens until newline or closing brace)
    return { kind: 'cmd', tokens: collectArgs() };
  }

  function parseIf() {
    eat(); // if
    const cond = collectUntil('{');
    const body = parseBlock();
    skipNL();
    let elifs = [], elseBody = null;
    while (at('WORD') && (peek().value === 'elif' || peek().value === 'else')) {
      const which = eat().value;
      if (which === 'elif') { const c = collectUntil('{'); elifs.push({ cond: c, body: parseBlock() }); skipNL(); }
      else { elseBody = parseBlock(); break; }
    }
    return { kind: 'if', cond, body, elifs, elseBody };
  }

  const program = [];
  skipNL();
  while (!at('EOF')) { program.push(parseStatement()); skipNL(); }
  return program;
}

// ── Evaluator helpers ───────────────────────────────────────────────
function makeScope(parent) {
  return { vars: new Map(), funcs: new Map(), parent: parent || null, get onError() { return this._oe || (parent && parent.onError) || 'stop'; }, set onError(v) { this._oe = v; } };
}
function lookup(scope, name) { let s = scope; while (s) { if (s.vars.has(name)) return s.vars.get(name); s = s.parent; } return undefined; }
function lookupFn(scope, name) { let s = scope; while (s) { if (s.funcs.has(name)) return s.funcs.get(name); s = s.parent; } return undefined; }

// Expand $var / ${var}. `lit` (optional) is the per-character mask from the
// tokenizer: a '$' whose mask character is '1' came from inside a single-quoted
// run and is literal text, not an expansion (SPEC §8). Passing no mask keeps the
// old unconditional behavior, which is right for text the script author wrote
// directly (an `echo` body) and wrong for anything that carries a value.
function interp(text, scope, lit) {
  const s = String(text);
  if (!lit) {
    return s.replace(/\$\{(\w+)\}|\$(\w+)/g, (_, a, b) => {
      const v = lookup(scope, a || b);
      return v === undefined ? '' : String(v);
    });
  }
  let out = '';
  for (let i = 0; i < s.length;) {
    if (s[i] !== '$' || lit[i] === '1') { out += s[i++]; continue; }
    const m = /^\$\{(\w+)\}|^\$(\w+)/.exec(s.slice(i));
    if (!m) { out += s[i++]; continue; }
    const v = lookup(scope, m[1] || m[2]);
    out += v === undefined ? '' : String(v);
    i += m[0].length;
  }
  return out;
}

// Evaluate an expression token list to a JS value (numbers, strings, bools,
// comparisons, &&, ||, !, parentheses). Bare $vars resolved; bare words = string.
function evalExpr(tokens, scope) {
  let i = 0;
  const peek = () => tokens[i];
  const next = () => tokens[i++];
  const truthy = (v) => v !== false && v !== 0 && v !== '' && v != null && v !== 'false';

  function primary() {
    const t = peek();
    if (!t) return '';
    if (t.type === '(') { next(); const v = orExpr(); if (peek() && peek().type === ')') next(); return v; }
    if (t.type === 'WORD') {
      next();
      let raw = t.value;
      if (!t.quoted && /^\$/.test(raw)) return lookup(scope, raw.slice(1).replace(/[{}]/g, ''));
      raw = interp(raw, scope, t.lit);
      if (!t.quoted) {
        if (raw === 'true') return true; if (raw === 'false') return false;
        if (raw !== '' && !isNaN(Number(raw))) return Number(raw);
      }
      return raw;
    }
    next(); return '';
  }
  function unary() {
    if (peek() && peek().type === 'WORD' && peek().value === '!') { next(); return !truthy(unary()); }
    return primary();
  }
  function compare() {
    let l = unary();
    while (peek() && peek().type === 'WORD' && ['==', '!=', '<', '>', '<=', '>='].includes(peek().value)) {
      const op = next().value; const r = unary();
      const ln = Number(l), rn = Number(r); const num = !isNaN(ln) && !isNaN(rn);
      if (op === '==') l = num ? ln === rn : String(l) === String(r);
      else if (op === '!=') l = num ? ln !== rn : String(l) !== String(r);
      else if (op === '<') l = num ? ln < rn : String(l) < String(r);
      else if (op === '>') l = num ? ln > rn : String(l) > String(r);
      else if (op === '<=') l = num ? ln <= rn : String(l) <= String(r);
      else if (op === '>=') l = num ? ln >= rn : String(l) >= String(r);
    }
    return l;
  }
  function andExpr() { let l = compare(); while (peek() && peek().value === '&&') { next(); const r = compare(); l = truthy(l) && truthy(r); } return l; }
  function orExpr() { let l = andExpr(); while (peek() && peek().value === '||') { next(); const r = andExpr(); l = truthy(l) || truthy(r); } return l; }
  return orExpr();
}

// Rebuild a command string from tokens, expanding $vars and re-quoting each
// token per §6.1. Re-quoting with quoteArg (rather than the old `"${v}"`, which
// a value containing a double quote walked straight out of) is what makes the
// round trip sound: splitArgs of the result yields exactly these tokens, so an
// expanded value is one argument and can never become syntax.
function tokensToCommand(tokens, scope) {
  return tokens.map(t => quoteArg(interp(t.value, scope, t.lit))).join(' ');
}
function hasOperator(tokens) {
  return tokens.some(t => t.type === 'WORD' && ['==', '!=', '<', '>', '<=', '>=', '&&', '||', '!'].includes(t.value));
}

// ── Executor ────────────────────────────────────────────────────────
async function runScript(src, { dispatch, logger, args = [] } = {}) {
  const program = parse(tokenize(src));
  const out = [];
  const emit = (s) => { out.push(s); if (logger) logger.info('script', s); };
  const root = makeScope(null);
  args.forEach((a, k) => root.vars.set(String(k + 1), a));
  root.vars.set('@', args.join(' '));

  async function evalCondition(tokens, scope) {
    if (hasOperator(tokens)) return !!evalExpr(tokens, scope);
    const r = await dispatch(tokensToCommand(tokens, scope));
    return r.ok;
  }
  async function captureRHS(tokens, scope) {
    // $( command )  or  expression
    if (tokens[0] && tokens[0].type === '(' || (tokens[0] && tokens[0].value === '$' )) { /* fallthrough */ }
    if (tokens.length >= 2 && tokens[0].value === '$' && tokens[1].type === '(') {
      const inner = tokens.slice(2, tokens.findIndex(t => t.type === ')'));
      const r = await dispatch(tokensToCommand(inner, scope));
      if (inner.some(t => t.value === '--json')) return r.data;
      return r.lines.join('\n').trim();
    }
    return evalExpr(tokens, scope);
  }

  async function exec(stmts, scope) {
    for (const st of stmts) {
      switch (st.kind) {
        case 'let': scope.vars.set(st.name, await captureRHS(st.expr, scope)); break;
        case 'echo': emit(interp(st.expr.map(t => t.value).join(' '), scope)); break;
        case 'onerror': scope.onError = st.mode; break;
        case 'wait': await new Promise(r => setTimeout(r, Number(interp(st.expr.map(t => t.value).join(''), scope)) || 0)); break;
        case 'halt': throw new Halt(Number(evalExpr(st.expr, scope)) || 0);
        case 'return': throw new Return(st.expr.length ? evalExpr(st.expr, scope) : undefined);
        case 'break': throw new Break();
        case 'continue': throw new Continue();
        case 'def': scope.funcs.set(st.name, st); break;
        case 'include': {
          const fs = require('fs');
          const file = interp(st.expr.map(t => t.value).join(''), scope);
          await exec(parse(tokenize(fs.readFileSync(file, 'utf8'))), scope);
          break;
        }
        case 'call': { const r = await dispatch('script run ' + tokensToCommand(st.expr, scope)); r.lines.forEach(emit); break; }
        case 'assert': {
          const ok = await evalCondition(st.cmd, scope);
          if (!ok) throw new Halt(1);
          break;
        }
        case 'if': {
          if (await evalCondition(st.cond, scope)) { await exec(st.body, makeScope(scope)); break; }
          let done = false;
          for (const e of st.elifs) { if (await evalCondition(e.cond, scope)) { await exec(e.body, makeScope(scope)); done = true; break; } }
          if (!done && st.elseBody) await exec(st.elseBody, makeScope(scope));
          break;
        }
        case 'while': {
          let guard = 0;
          while (await evalCondition(st.cond, scope)) {
            if (guard++ > 100000) throw new Error('while loop guard tripped');
            try { await exec(st.body, makeScope(scope)); } catch (e) { if (e instanceof Break) break; if (e instanceof Continue) continue; throw e; }
          }
          break;
        }
        case 'repeat': {
          const c = Number(evalExpr(st.count, scope)) || 0;
          for (let k = 0; k < c; k++) {
            try { await exec(st.body, makeScope(scope)); } catch (e) { if (e instanceof Break) break; if (e instanceof Continue) continue; throw e; }
          }
          break;
        }
        case 'foreach': {
          const r = await dispatch(tokensToCommand(st.cmd, scope));
          const items = Array.isArray(r.data) ? r.data : (r.lines || []);
          for (const item of items) {
            const inner = makeScope(scope); inner.vars.set(st.var, item);
            try { await exec(st.body, inner); } catch (e) { if (e instanceof Break) break; if (e instanceof Continue) continue; throw e; }
          }
          break;
        }
        case 'try': {
          try { await exec(st.body, makeScope(scope)); }
          catch (e) {
            if (e instanceof Halt || e instanceof Return) throw e;
            const inner = makeScope(scope); inner.vars.set(st.catchVar, e.message || String(e));
            await exec(st.handler, inner);
          }
          break;
        }
        case 'fcall': {
          const fn = lookupFn(scope, st.name);
          if (!fn) { // maybe it's a saved script
            const r = await dispatch(`script run ${st.name} ${st.args.map(a => tokensToCommand(a, scope)).join(' ')}`);
            r.lines.forEach(emit); break;
          }
          const inner = makeScope(scope);
          fn.params.forEach((pn, idx) => inner.vars.set(pn, st.args[idx] ? evalExpr(st.args[idx], scope) : ''));
          try { await exec(fn.body, inner); } catch (e) { if (e instanceof Return) { /* value discarded at stmt level */ } else throw e; }
          break;
        }
        case 'cmd': {
          if (!st.tokens.length) break;
          const cmd = tokensToCommand(st.tokens, scope);
          const r = await dispatch(cmd);
          (r.lines || []).forEach(l => out.push(l));
          // a failed command throws a catchable error (try/catch can handle it);
          // `on error continue` swallows it so the script keeps going.
          if (!r.ok && scope.onError === 'stop') {
            throw new Error((r.lines && r.lines[r.lines.length - 1]) || `command failed: ${cmd}`);
          }
          break;
        }
        default: break;
      }
    }
  }

  try {
    await exec(program, root);
    return { ok: true, lines: out, data: null };
  } catch (e) {
    if (e instanceof Halt) return { ok: e.code === 0, lines: out, data: { code: e.code } };
    if (e instanceof Return) return { ok: true, lines: out, data: e.value };
    if (logger) logger.error('script', e.message);
    return { ok: false, lines: [...out, `script error: ${e.message}`], data: null };
  }
}

module.exports = { runScript, tokenize, parse };
