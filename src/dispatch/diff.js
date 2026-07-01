'use strict';
// Deep clone + a rune-level diff used by status / diff / undo.

const clone = (x) => JSON.parse(JSON.stringify(x));

// Compare two mantles by rune spirit.id. Returns { added, removed, changed }.
// `changed` entries list which top-level rune keys differ.
function diffMantle(base, cur) {
  const baseById = new Map((base ? base.runes : []).map(r => [r.spirit.id, r]));
  const curById = new Map((cur ? cur.runes : []).map(r => [r.spirit.id, r]));
  const added = [], removed = [], changed = [];

  for (const [id, r] of curById) {
    if (!baseById.has(id)) { added.push(r.spirit.name); continue; }
    const b = baseById.get(id);
    const keys = ['glyph', 'facets', 'tags', 'content', 'placement', 'relations', 'spirit'];
    const fields = keys.filter(k => JSON.stringify(b[k]) !== JSON.stringify(r[k]));
    if (fields.length) changed.push({ name: r.spirit.name, fields });
  }
  for (const [id, r] of baseById) {
    if (!curById.has(id)) removed.push(r.spirit.name);
  }
  return { added, removed, changed };
}

function isDirty(d) {
  return d.added.length > 0 || d.removed.length > 0 || d.changed.length > 0;
}

module.exports = { clone, diffMantle, isDirty };
