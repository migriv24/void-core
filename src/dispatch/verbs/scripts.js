'use strict';
// verbs/scripts.js — the "scripts" verb family (the script verb) for the dispatcher.
// A factory over shared deps; mirrors the C core's verbs_scripts.c split. Handler
// bodies are unchanged from the original monolithic dispatch.js.
module.exports = (deps) => {
  const { ctx, state, store, glyphs, logger, adapters, config, res, clone, diffMantle,
          isDirty, M, T, AX, createRune, createBinding, parseRef, fs, path, spawn,
          mantleOrThrow, runeOrThrow, targets, coerce } = deps;
  return {
    async script(c, pos, flags, dispatch) {
      const sub = pos[0];
      if (sub === 'ls') return res(Object.keys(state.scripts).length ? Object.keys(state.scripts) : ['(no scripts)'], Object.keys(state.scripts));
      if (sub === 'show') return res([state.scripts[pos[1]] || `(no script "${pos[1]}")`], state.scripts[pos[1]]);
      if (sub === 'new') { state.scripts[pos[1]] = state.scripts[pos[1]] || ''; return res([`created script ${pos[1]}`]); }
      if (sub === 'set') { state.scripts[pos[1]] = fs.readFileSync(pos[2], 'utf8'); return res([`loaded script ${pos[1]} from ${pos[2]}`]); }
      if (sub === 'run') {
        const name = pos[1];
        const src = state.scripts[name] || (fs.existsSync(name) ? fs.readFileSync(name, 'utf8') : null);
        if (src == null) throw new Error(`no script "${name}"`);
        const { runScript } = require('../../scripts/voidscript');
        return await runScript(src, { dispatch, logger, args: pos.slice(2) });
      }
      throw new Error('usage: script ls|show|new|set|run ...');
    },
  };
};
