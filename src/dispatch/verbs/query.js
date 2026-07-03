'use strict';
// verbs/query.js — the "query" verb family (read-only inspection verbs) for the dispatcher.
// A factory over shared deps; mirrors the C core's verbs_query.c split. Handler
// bodies are unchanged from the original monolithic dispatch.js.
module.exports = (deps) => {
  const { ctx, state, store, glyphs, logger, adapters, config, res, clone, diffMantle,
          isDirty, M, T, AX, createRune, createBinding, parseRef, fs, path, spawn,
          mantleOrThrow, runeOrThrow, targets, coerce } = deps;
  return {
    describe(c, pos) {
      const m = mantleOrThrow();
      if (pos[0]) {
        const r = runeOrThrow(pos[0]);
        const g = glyphs.get(r.glyph);
        const binds = state.bindings.filter(b =>
          (b.from.mantle === m.name && b.from.rune === r.spirit.name) ||
          (b.to.mantle === m.name && b.to.rune === r.spirit.name));
        const lines = [
          `${r.spirit.name}  [${r.glyph}]  (${r.spirit.id})`,
          `  summary: ${g ? g.describe(r) : JSON.stringify(r.content)}`,
          `  tags: ${r.tags.join(', ') || '(none)'}`,
          '  facets:',
          ...['who', 'what', 'when', 'where', 'why', 'how'].map(k => `    ${k}: ${r.facets[k] || ''}`),
          ...(binds.length ? ['  bindings:', ...binds.map(b => `    ${b.from.mantle}:${b.from.rune} --${b.from.on}--> ${b.to.mantle}:${b.to.rune}`)] : []),
        ];
        return res(lines, r);
      }
      const lines = [`mantle "${m.name}" over domain "${m.domain || '(none)'}" — ${m.runes.length} rune(s)`];
      for (const r of m.runes) {
        const g = glyphs.get(r.glyph);
        lines.push(`  • ${r.spirit.name} [${r.glyph}] — ${(g ? g.describe(r) : '').slice(0, 80)}`);
      }
      return res(lines, m);
    },
    ls(c, pos, flags) {
      const m = ctx.active();
      if (!m) {  // root-ls (SPEC §7): no active mantle -> list the mantles; data = mantle names
        const lines = state.mantles.map(mm => `  ${mm.name}/`);
        return res(lines.length ? lines : ["(no mantles — create one with 'mantle new <name>')"],
                   state.mantles.map(mm => mm.name));
      }
      const runes = flags.tag ? T.filter(m, flags.tag) : m.runes;
      const lines = runes.map(r => `${r.spirit.name}  [${r.glyph}]  ${r.tags.length ? '#' + r.tags.join(' #') : ''}`.trim());
      return res(lines.length ? lines : ['(no runes)'], runes.map(r => r.spirit.name));
    },
    tree(c) {
      const m = mantleOrThrow();
      const lines = [`${m.name}`];
      for (const r of m.runes) {
        lines.push(`├─ ${r.spirit.name} [${r.glyph}]`);
        for (const e of (m.layout.edges || []).filter(e => e.from === r.spirit.name)) {
          lines.push(`│   ↳ ${e.relation} ${e.to}`);
        }
      }
      return res(lines, m.layout);
    },
    get(c, pos) {
      const r = runeOrThrow(pos[0]);
      if (pos[1]) return res([String(r.content[pos[1]] ?? '')], r.content[pos[1]]);
      return res([JSON.stringify(r.content, null, 2)], r.content);
    },
    find(c, pos) {
      const q = (pos.join(' ') || '').toLowerCase();
      const m = mantleOrThrow();
      const hits = m.runes.filter(r =>
        r.spirit.name.toLowerCase().includes(q) ||
        JSON.stringify(r.content).toLowerCase().includes(q) ||
        JSON.stringify(r.facets).toLowerCase().includes(q) ||
        r.tags.some(t => t.toLowerCase().includes(q)));
      return res(hits.map(r => `${r.spirit.name} [${r.glyph}]`), hits.map(r => r.spirit.name));
    },
    cat(c, pos) {
      const r = runeOrThrow(pos[0]);
      return res([JSON.stringify(r, null, 2)], r);
    },
    status(c, pos, flags) {
      const d = diffMantle(ctx.baseline.find(b => b.name === ctx.active().name), ctx.active());
      if (flags.dirty) return res([], d, isDirty(d));   // for script truthiness
      if (!isDirty(d)) return res(['clean — no unsaved changes'], d);
      const lines = [];
      if (d.added.length) lines.push(`added:   ${d.added.join(', ')}`);
      if (d.removed.length) lines.push(`removed: ${d.removed.join(', ')}`);
      for (const ch of d.changed) lines.push(`changed: ${ch.name} (${ch.fields.join(', ')})`);
      return res(lines, d);
    },
    diff(c, pos) {
      const base = ctx.baseline.find(b => b.name === ctx.active().name);
      if (pos[0]) {
        const before = base ? M.findRune(base, pos[0]) : null;
        const after = M.findRune(ctx.active(), pos[0]);
        return res([
          '--- saved', JSON.stringify(before, null, 2),
          '+++ working', JSON.stringify(after, null, 2),
        ], { before, after });
      }
      return deps.handlers.status(c, pos, {});
    },
    history(c, pos, flags) {
      const n = Number(flags.tail) || ctx.undoStack.length;
      const lines = ctx.undoStack.slice(-n).map((u, i) => `${i + 1}. ${u.label}`);
      return res(lines.length ? lines : ['(no history)'], ctx.undoStack.map(u => u.label));
    },
    glyphs(c) {
      return res(glyphs.list().map(g => `${g.glyph}  (${g.editor})  fields: ${g.fields.join(', ')}`), glyphs.list());
    },
    axes(c, pos, flags) {
      if (pos[0] === 'all') {
        return res(Object.entries(AX.AXES).map(([a, d]) => `${a} — ${d}`), AX.AXES);
      }
      const m = mantleOrThrow();
      const all = new Set();
      for (const r of m.runes) for (const t of r.tags) all.add(t);
      const buckets = AX.byAxis([...all]);
      const lines = Object.keys(AX.AXES).filter(a => buckets[a]).map(a => `${a}: ${buckets[a].join(', ')}`);
      return res(lines.length ? lines : ['(no tags)'], buckets);
    },
    mantles(c) {
      const lines = state.mantles.map(m =>
        `${m === ctx.active() ? '* ' : '  '}${m.name}  [${m.runes.length} runes]  domain: ${m.domain || '-'}`);
      return res(lines.length ? lines : ['(no mantles)'], state.mantles.map(m => m.name));
    },
    domain(c) {
      const d = ctx.activeDomain();
      if (!d) return res(['(no active domain)'], null);
      return res([
        `name:    ${d.name}`, `repo:    ${d.repo || '-'}`, `live:    ${d.liveUrl || '-'}`,
        `build:   ${d.build || '-'}`, `deploy:  ${d.deploy || '-'}`,
        `preview: ${d.preview || '-'}`, `port:    ${d.port || '-'}`,
      ], d);
    },
    where(c) {
      const m = ctx.active(), d = ctx.activeDomain();
      return res([`mantle: ${m ? m.name : '(none)'}   domain: ${d ? d.name : '(none)'}`], { mantle: m && m.name, domain: d && d.name });
    },
    validate(c, pos, flags) {
      const m = mantleOrThrow();
      const errs = [];
      const names = new Set(m.runes.map(r => r.spirit.name));
      const seen = new Set();
      for (const r of m.runes) {
        if (seen.has(r.spirit.name)) errs.push(`duplicate rune name: ${r.spirit.name}`);
        seen.add(r.spirit.name);
        if (!glyphs.has(r.glyph)) errs.push(`rune ${r.spirit.name}: unregistered glyph "${r.glyph}"`);
      }
      for (const e of m.layout.edges || []) {
        if (!names.has(e.from)) errs.push(`layout edge from missing rune "${e.from}"`);
        if (!names.has(e.to)) errs.push(`layout edge to missing rune "${e.to}"`);
      }
      if (flags.quiet) return res([], errs, errs.length === 0);
      return res(errs.length ? errs : ['valid — no problems found'], errs, errs.length === 0);
    },
  };
};
