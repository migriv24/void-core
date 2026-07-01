'use strict';
// Void Core entry point (JS prototype — kept as the CLI/dispatcher conformance
// oracle for the C core; see core/README.md). A twin calls createManager() with
// its state file, extra glyphs, site-sync adapters, and domain(s). Returns a
// wired manager (dispatch + ctx + logger) that the CLI drives. See ARCHITECTURE.md §8.

const path = require('path');
const { createStore } = require('./model/store');
const { createDomain } = require('./model/domain');
const { createMantle } = require('./model/mantle');
const { createRune } = require('./model/rune');
const { createRegistry, registerBuiltins } = require('./glyphs/registry');
const { createLogger } = require('./log/logger');
const { createDispatcher } = require('./dispatch/dispatch');

function createManager(opts = {}) {
  const stateFile = opts.stateFile || path.resolve(process.cwd(), 'state.json');
  const logFile = opts.logFile || path.join(path.dirname(stateFile), 'logs', 'void.log');

  const logger = createLogger({ file: logFile });
  const store = createStore(stateFile);
  const state = store.load();

  // Glyph registry: built-ins, then twin extras.
  const glyphs = registerBuiltins(createRegistry());
  for (const g of opts.glyphs || []) glyphs.register(g);

  // Seed domains / mantles passed in by the twin (only if state is empty).
  for (const d of opts.domains || []) {
    if (!state.domains[d.name]) state.domains[d.name] = createDomain(d);
    if (!state.active.domain) state.active.domain = d.name;
  }
  for (const m of opts.mantles || []) {
    if (!state.mantles.find(x => x.name === m.name)) state.mantles.push(createMantle(m));
    if (!state.active.mantle) state.active.mantle = m.name;
  }
  if (!state.active.mantle && state.mantles[0]) state.active.mantle = state.mantles[0].name;

  const { dispatch, ctx, handlers } = createDispatcher({
    state, store, glyphs, logger,
    adapters: opts.adapters || {},
    config: state.config,
  });

  // Let a twin run one-time setup (e.g. import from real site files on first run).
  if (typeof opts.bootstrap === 'function') opts.bootstrap({ state, store, glyphs, logger, dispatch, ctx });

  // First-ever load has no saved baseline; treat the just-loaded (post-bootstrap)
  // state as the clean baseline so dirty-tracking works across CLI processes.
  if (state._baseline === undefined) {
    state._baseline = JSON.parse(JSON.stringify(state.mantles));
    ctx.baseline = JSON.parse(JSON.stringify(state.mantles));
  }

  return { dispatch, ctx, handlers, logger, glyphs, store, state, stateFile };
}

module.exports = {
  createManager,
  // building blocks, re-exported so twins can use them directly
  createStore, createDomain, createMantle, createRune,
  createRegistry, registerBuiltins, createLogger, createDispatcher,
  runScript: require('./scripts/voidscript').runScript,
};
