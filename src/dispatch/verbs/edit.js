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
    place(c, pos, flags) {
      // SPEC §3.2/§6/§7 — the view slice: place <ref> reads placement,
      // place <ref> <x> <y> [<z>] sets it, place <ref> --clear nulls it.
      // Single rune only. On the mutation spine (logged) but NOT undoable:
      // no pushUndo, and undo/redo carries placements over (see undo below).
      const [ref, x, y, z] = pos;
      if (!ref) throw new Error('usage: place <rune> [<x> <y> [<z>] | --clear]');
      const r = runeOrThrow(ref);
      if (flags && flags.clear) {
        r.placement = null;
        logger.info('place', `place ${ref} --clear`);
        return res([`${r.spirit.name} placement cleared`]);
      }
      if (x === undefined)   // query
        return res([`${r.spirit.name} @ ${r.placement ? JSON.stringify(r.placement) : '(unplaced)'}`],
                   clone(r.placement));
      const nx = Number(x), ny = Number(y);
      if (y === undefined || x === '' || y === '' || isNaN(nx) || isNaN(ny))
        throw new Error('place: coordinates must be numbers');
      const p = { x: nx, y: ny };
      if (z !== undefined) {
        const nz = Number(z);
        if (z === '' || isNaN(nz)) throw new Error('place: coordinates must be numbers');
        p.z = nz;
      }
      r.placement = clone(p);
      logger.info('place', `place ${pos.join(' ')}`);
      return res([`${r.spirit.name} @ ${JSON.stringify(p)}`], p);
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
    // SPEC §3.4/§7.2 — the mantle lifecycle family (the mantle-level analogues
    // of rune new|rm|rename). All three mutate the undoable slice.
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
      if (pos[0] === 'rm') {
        // Removing the ACTIVE mantle deactivates (the `use` / `cd /` cold-start
        // semantics) rather than refusing; removing the last one is allowed.
        const name = pos[1];
        if (!name) throw new Error('usage: mantle rm <name>');
        const i = state.mantles.findIndex(m => m.name === name);
        if (i < 0) throw new Error(`no mantle "${name}"`);
        ctx.pushUndo(`mantle rm ${name}`);
        state.mantles.splice(i, 1);
        if (state.active.mantle === name) {
          state.active.mantle = null;
          return res([`removed mantle ${name} (no active mantle)`]);
        }
        return res([`removed mantle ${name}`]);
      }
      if (pos[0] === 'rename') {
        const [, oldName, newName] = pos;
        if (!oldName || !newName) throw new Error('usage: mantle rename <old> <new>');
        const m = state.mantles.find(x => x.name === oldName);
        if (!m) throw new Error(`no mantle "${oldName}"`);
        if (state.mantles.find(x => x.name === newName)) throw new Error(`mantle "${newName}" exists`);
        ctx.pushUndo(`mantle rename ${oldName}`);
        m.name = newName;
        if (state.active.mantle === oldName) state.active.mantle = newName;
        return res([`renamed mantle ${oldName} -> ${newName}`]);
      }
      return deps.handlers.mantles(c);
    },
    undo(c, pos) {
      const n = Number(pos[0]) || 1;
      let done = 0;
      for (let i = 0; i < n && ctx.undoStack.length; i++) {
        ctx.redoStack.push({ mantles: clone(state.mantles), active: clone(state.active), label: 'redo' });
        const snap = ctx.undoStack.pop();
        overlayPlacements(snap.mantles, state.mantles);
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
        overlayPlacements(snap.mantles, state.mantles);
        state.mantles = snap.mantles; state.active = snap.active; done++;
      }
      return res([`redid ${done} step(s)`]);
    },
  };

  // The view slice (SPEC §3.2/§6): placement is OUTSIDE the undo slice. Before a
  // snapshot lands, carry each surviving rune's CURRENT placement into it
  // (matched by mantle name + rune name), so undo/redo never moves what the
  // user placed. Runes only in the snapshot keep their snapshot placement.
  function overlayPlacements(incoming, current) {
    for (const im of incoming) {
      const cm = current.find(m => m.name === im.name);
      if (!cm) continue;
      for (const ir of im.runes) {
        const cr = cm.runes.find(r => r.spirit.name === ir.spirit.name);
        if (cr) ir.placement = cr.placement ? clone(cr.placement) : null;
      }
    }
  }
};
