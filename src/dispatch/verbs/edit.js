'use strict';
// verbs/edit.js — the "edit" verb family (model-mutating verbs (+ undo/redo)) for the dispatcher.
// A factory over shared deps; mirrors the C core's verbs_edit.c split. Handler
// bodies are unchanged from the original monolithic dispatch.js.
module.exports = (deps) => {
  const { ctx, state, store, glyphs, logger, adapters, config, res, clone, diffMantle,
          isDirty, M, T, AX, createRune, createBinding, parseRef, fs, path, spawn,
          mantleOrThrow, runeOrThrow, targets, coerce } = deps;
  return {
    set(c, pos) {
      const [ref, field, ...rest] = pos;
      if (!field) throw new Error('usage: set <name> <field> <value>');
      ctx.pushUndo(`set ${ref} ${field}`);
      const value = coerce(rest.join(' '), {});
      const list = targets(ref);
      for (const r of list) r.content[field] = value;
      logger.info('set', `${list.map(r => r.spirit.name).join(', ')} .${field} = ${JSON.stringify(value)}`);
      return res([`set ${field} on ${list.length} rune(s)`], list.map(r => r.spirit.name));
    },
    facet(c, pos) {
      const [ref, key, ...rest] = pos;
      if (!['who', 'what', 'when', 'where', 'why', 'how'].includes(key)) throw new Error('facet must be who|what|when|where|why|how');
      ctx.pushUndo(`facet ${ref} ${key}`);
      for (const r of targets(ref)) r.facets[key] = rest.join(' ');
      return res([`set facet ${key}`]);
    },
    tag(c, pos) {
      const [ref, ...ops] = pos;
      ctx.pushUndo(`tag ${ref}`);
      const add = ops.filter(o => o.startsWith('+')).map(o => o.slice(1));
      const rem = ops.filter(o => o.startsWith('-')).map(o => o.slice(1));
      for (const r of targets(ref)) { T.addTags(r, add); T.removeTags(r, rem); }
      return res([`tags updated (+${add.length} -${rem.length})`]);
    },
    rune(c, pos) {
      const sub = pos[0];
      const m = mantleOrThrow();
      if (sub === 'new') {
        const [, glyph, name] = pos;
        if (!glyph || !name) throw new Error('usage: rune new <glyph> <name>');
        if (!glyphs.has(glyph)) throw new Error(`unknown glyph "${glyph}" (see \`glyphs\`)`);
        ctx.pushUndo(`rune new ${name}`);
        const r = createRune({ name, glyph, content: glyphs.get(glyph).newContent() });
        M.addRune(m, r);
        logger.info('rune', `minted ${name} [${glyph}] ${r.spirit.id}`);
        return res([`created ${name} (${r.spirit.id})`], r);
      }
      if (sub === 'rm') {
        ctx.pushUndo(`rune rm ${pos[1]}`);
        if (!M.removeRune(m, pos[1])) throw new Error(`no rune "${pos[1]}"`);
        return res([`removed ${pos[1]}`]);
      }
      if (sub === 'rename') {
        ctx.pushUndo(`rune rename ${pos[1]}`);
        M.renameRune(m, pos[1], pos[2]);
        return res([`renamed ${pos[1]} -> ${pos[2]}`]);
      }
      if (sub === 'dup') {
        const src = runeOrThrow(pos[1]);
        ctx.pushUndo(`rune dup ${pos[1]}`);
        const name = pos[2] || `${src.spirit.name}-copy`;
        const r = createRune({ name, glyph: src.glyph, facets: clone(src.facets), tags: [...src.tags], content: clone(src.content) });
        M.addRune(m, r);
        return res([`duplicated -> ${name}`], r);
      }
      if (sub === 'move') {
        const [, ref, relation, target] = pos;
        runeOrThrow(ref); runeOrThrow(target);
        ctx.pushUndo(`rune move ${ref}`);
        m.layout.edges = (m.layout.edges || []).filter(e => !(e.from === ref && e.to === target));
        m.layout.edges.push({ from: ref, to: target, relation });
        return res([`${ref} ${relation} ${target}`]);
      }
      throw new Error('usage: rune new|rm|rename|dup|move ...');
    },
    mantle(c, pos) {
      if (pos[0] === 'new') {
        const name = pos[1];
        if (!name) throw new Error('usage: mantle new <name>');
        if (state.mantles.find(m => m.name === name)) throw new Error(`mantle "${name}" exists`);
        ctx.pushUndo(`mantle new ${name}`);
        const mantle = M.createMantle({ name, domain: state.active.domain });
        state.mantles.push(mantle);
        state.active.mantle = name;
        return res([`created mantle ${name} (active)`], mantle);
      }
      return deps.handlers.mantles(c);
    },
    undo(c, pos) {
      const n = Number(pos[0]) || 1;
      let done = 0;
      for (let i = 0; i < n && ctx.undoStack.length; i++) {
        ctx.redoStack.push({ mantles: clone(state.mantles), active: clone(state.active), label: 'redo' });
        const snap = ctx.undoStack.pop();
        state.mantles = snap.mantles; state.active = snap.active; done++;
      }
      return res([`undid ${done} step(s)`]);
    },
    redo(c, pos) {
      const n = Number(pos[0]) || 1;
      let done = 0;
      for (let i = 0; i < n && ctx.redoStack.length; i++) {
        ctx.undoStack.push({ mantles: clone(state.mantles), active: clone(state.active), label: 'undo' });
        const snap = ctx.redoStack.pop();
        state.mantles = snap.mantles; state.active = snap.active; done++;
      }
      return res([`redid ${done} step(s)`]);
    },
  };
};
