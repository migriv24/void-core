'use strict';
// The one command dispatcher (ARCHITECTURE §4): the thin router. The CLI and the
// script runner call this. Mutations are undoable and dirty-tracked. The verb
// handlers live in verbs/<family>.js (query/edit/graph/lifecycle/scripts/system),
// mirroring the C core's verbs_*.c split; this file wires them over shared deps.

const fs = require('fs');
const path = require('path');
const { spawn } = require('child_process');
const { splitArgs, parseArgs } = require('./args');
const { clone, diffMantle, isDirty } = require('./diff');
const { createRune } = require('../model/rune');
const { createBinding, parseRef } = require('../model/binding');
const M = require('../model/mantle');
const T = require('../tags/tags');
const AX = require('../tags/axes');

const res = (lines = [], data = null, ok = true) => ({ ok, lines: Array.isArray(lines) ? lines : [lines], data });

function createDispatcher({ state, store, glyphs, logger, adapters = {}, config = {} }) {
  const ctx = {
    state, store, glyphs, logger, adapters, config,
    baseline: clone(state._baseline || state.mantles),  // last Save Progress (dirty-tracking)
    undoStack: [], redoStack: [],
    preview: null,                       // running preview child process
    active() {
      // Strict by-name resolve (matches the C core's vc_active_mantle): no
      // fallback to mantles[0], so "no active mantle" is a real state and the
      // SPEC §7 root-ls / `use` deactivation semantics hold.
      return state.mantles.find(m => m.name === state.active.mantle) || null;
    },
    activeDomain() {
      const m = ctx.active();
      const name = (m && m.domain) || state.active.domain;
      return name ? state.domains[name] : null;
    },
    pushUndo(label) {
      ctx.undoStack.push({ mantles: clone(state.mantles), active: clone(state.active), label });
      if (ctx.undoStack.length > 200) ctx.undoStack.shift();
      ctx.redoStack.length = 0;
    },
  };

  function mantleOrThrow() {
    const m = ctx.active();
    if (!m) throw new Error('no active mantle — create one with `mantle new <name>`');
    return m;
  }
  function runeOrThrow(ref) {
    const r = M.findRune(mantleOrThrow(), ref);
    if (!r) throw new Error(`no rune "${ref}" in mantle "${ctx.active().name}"`);
    return r;
  }
  // Resolve a target: a single name/id, or @<tag-expr> selecting many.
  function targets(ref) {
    const m = mantleOrThrow();
    if (typeof ref === 'string' && ref.startsWith('@')) return T.filter(m, ref.slice(1));
    return [runeOrThrow(ref)];
  }
  function coerce(v, flags) {
    if (flags && flags.json) { try { return JSON.parse(v); } catch { return v; } }
    if (v === 'true') return true;
    if (v === 'false') return false;
    if (v !== '' && !isNaN(Number(v)) && /^-?\d/.test(v)) return Number(v);
    return v;
  }

  // POSIX surface (SPEC §7): aliases are argument-aware desugarings applied to
  // the argv before routing — one semantics, many spellings; an alias never
  // forks behavior. `renames` swap the verb; `desugars` splice in the family
  // verb so `rm x` means `rune rm x` (not `rune x`).
  const renames = { '?': 'help', man: 'help', quit: 'exit', pwd: 'where', dump: 'export', grep: 'find', cd: 'use' };
  const desugars = { rm: ['rune', 'rm'], mv: ['rune', 'rename'], cp: ['rune', 'dup'], mkdir: ['mantle', 'new'] };

  // Shared surface handed to every verb-family factory. `handlers` is filled in
  // just below so a handler can delegate to a sibling (e.g. mantle -> mantles) at
  // call time.
  const deps = {
    ctx, state, store, glyphs, logger, adapters, config,
    res, clone, diffMantle, isDirty, M, T, AX,
    createRune, createBinding, parseRef, fs, path, spawn,
    mantleOrThrow, runeOrThrow, targets, coerce,
    handlers: null,
  };

  const handlers = Object.assign(
    {},
    require('./verbs/query')(deps),
    require('./verbs/edit')(deps),
    require('./verbs/graph')(deps),
    require('./verbs/lifecycle')(deps),
    require('./verbs/scripts')(deps),
    require('./verbs/system')(deps),
  );
  deps.handlers = handlers;

  async function dispatch(input) {
    let argv = Array.isArray(input) ? input : splitArgs(String(input));
    if (!argv.length) return res([]);
    if (renames[argv[0]]) argv = [renames[argv[0]], ...argv.slice(1)];
    else if (desugars[argv[0]]) argv = [...desugars[argv[0]], ...argv.slice(1)];
    const verb = argv[0];
    const { positional, flags } = parseArgs(argv.slice(1));
    const handler = handlers[verb];
    if (!handler) { logger.warn('dispatch', `unknown command: ${verb}`); return res([`unknown command: ${verb} (try \`help\`)`], null, false); }
    try {
      const out = await handler(ctx, positional, flags, dispatch);
      return res(out && out.lines, out ? out.data : null, out ? out.ok : true);
    } catch (e) {
      logger.error(verb, e.message);
      return res([`error: ${e.message}`], null, false);
    }
  }

  return { dispatch, ctx, handlers };
}

module.exports = { createDispatcher };
