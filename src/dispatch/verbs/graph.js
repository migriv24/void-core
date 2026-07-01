'use strict';
// verbs/graph.js — the "graph" verb family (cross-rune bindings) for the dispatcher.
// A factory over shared deps; mirrors the C core's verbs_graph.c split. Handler
// bodies are unchanged from the original monolithic dispatch.js.
module.exports = (deps) => {
  const { ctx, state, store, glyphs, logger, adapters, config, res, clone, diffMantle,
          isDirty, M, T, AX, createRune, createBinding, parseRef, fs, path, spawn,
          mantleOrThrow, runeOrThrow, targets, coerce } = deps;
  return {
    bind(c, pos, flags) {
      const [fromRef, on, toRef, doEffect] = pos;
      if (!fromRef || !toRef) throw new Error('usage: bind <mantle:rune> <on> <mantle:rune> <do> [--name x]');
      const from = parseRef(fromRef, ctx.active() && ctx.active().name);
      const to = parseRef(toRef, ctx.active() && ctx.active().name);
      // validate the referenced runes exist
      for (const end of [from, to]) {
        const m = state.mantles.find(x => x.name === end.mantle);
        if (!m) throw new Error(`no mantle "${end.mantle}"`);
        if (end.rune && !M.findRune(m, end.rune)) throw new Error(`no rune "${end.rune}" in "${end.mantle}"`);
      }
      ctx.pushUndo(`bind ${fromRef}->${toRef}`);
      const b = createBinding({ from, to, on: on || 'reach', doEffect: doEffect || 'fire', name: flags.name, note: flags.note });
      state.bindings.push(b);
      logger.info('bind', `${b.from.mantle}:${b.from.rune} --${b.from.on}--> ${b.to.mantle}:${b.to.rune} (${b.to.do})`);
      return res([`bound ${b.id}${b.name ? ' (' + b.name + ')' : ''}`], b);
    },
    bindings(c, pos, flags) {
      let list = state.bindings;
      if (flags.mantle) list = list.filter(b => b.from.mantle === flags.mantle || b.to.mantle === flags.mantle);
      if (pos[0]) list = list.filter(b => b.from.rune === pos[0] || b.to.rune === pos[0]);
      const lines = list.map(b => `${b.id}${b.name ? ' ' + b.name : ''}: ${b.from.mantle}:${b.from.rune} --${b.from.on}--> ${b.to.mantle}:${b.to.rune} [${b.to.do}]`);
      return res(lines.length ? lines : ['(no bindings)'], list);
    },
    unbind(c, pos) {
      const key = pos[0];
      const before = state.bindings.length;
      ctx.pushUndo(`unbind ${key}`);
      state.bindings = state.bindings.filter(b => b.id !== key && b.name !== key);
      if (state.bindings.length === before) throw new Error(`no binding "${key}"`);
      return res([`removed ${key}`]);
    },
  };
};
