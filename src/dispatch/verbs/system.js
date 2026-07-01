'use strict';
// verbs/system.js — the "system" verb family (system / session verbs) for the dispatcher.
// A factory over shared deps; mirrors the C core's verbs_system.c split. Handler
// bodies are unchanged from the original monolithic dispatch.js.
module.exports = (deps) => {
  const { ctx, state, store, glyphs, logger, adapters, config, res, clone, diffMantle,
          isDirty, M, T, AX, createRune, createBinding, parseRef, fs, path, spawn,
          mantleOrThrow, runeOrThrow, targets, coerce } = deps;
  return {
    log(c, pos, flags) {
      const n = Number(flags.tail) || 200;
      return res(logger.tail(n, flags.level).map(l => l.line), logger.tail(n, flags.level));
    },
    use(c, pos) {
      const name = pos[0];
      if (state.mantles.find(m => m.name === name)) { state.active.mantle = name; return res([`active mantle -> ${name}`]); }
      if (state.domains[name]) { state.active.domain = name; return res([`active domain -> ${name}`]); }
      throw new Error(`no mantle or domain named "${name}"`);
    },
    config(c, pos) {
      if (pos[0] === 'set') { state.config[pos[1]] = coerce(pos.slice(2).join(' '), {}); return res([`config ${pos[1]} = ${state.config[pos[1]]}`]); }
      if (pos[0] === 'get') return res([String(state.config[pos[1]] ?? '')], state.config[pos[1]]);
      return res(Object.entries(state.config).map(([k, v]) => `${k} = ${v}`), state.config);
    },
    export(c, pos) {
      const m = mantleOrThrow();
      const bindings = state.bindings.filter(b => b.from.mantle === m.name || b.to.mantle === m.name);
      const out = { mantle: m, domain: ctx.activeDomain(), bindings };
      if (pos[0]) { fs.writeFileSync(pos[0], JSON.stringify(out, null, 2)); return res([`exported to ${pos[0]}`]); }
      return res([JSON.stringify(out, null, 2)], out);
    },
    import(c, pos) {
      const data = JSON.parse(fs.readFileSync(pos[0], 'utf8'));
      ctx.pushUndo(`import ${pos[0]}`);
      const mantle = M.createMantle(data.mantle || data);
      const i = state.mantles.findIndex(m => m.name === mantle.name);
      if (i >= 0) state.mantles[i] = mantle; else state.mantles.push(mantle);
      state.active.mantle = mantle.name;
      return res([`imported mantle ${mantle.name}`], mantle);
    },
    version() { return res([`void-core ${require('../../../package.json').version}`]); },
    exit() { return res(['bye'], { signal: 'exit' }); },
    help(c, pos) {
      if (pos[0] && deps.handlers[pos[0]]) return res([`\`${pos[0]}\` — see ARCHITECTURE.md §4`]);
      return res([
        'Void Core commands (see ARCHITECTURE.md §4):',
        ' read:    describe ls tree get find cat status diff history glyphs axes mantles domain validate where',
        ' mutate:  set facet tag rune mantle bind bindings unbind undo redo batch',
        ' life:    preview save deploy build revert',
        ' scripts: script run|ls|show|new|set',
        ' system:  log use config export import help version exit',
      ], Object.keys(deps.handlers));
    },
  };
};
