'use strict';
// Rune — the atomic editable unit. See ARCHITECTURE.md §2.1 and Codex §2.
// A plain serializable object so it lives in JSON and diffs cleanly in git.

const { createSpirit } = require('./spirit');

const FACET_KEYS = ['who', 'what', 'when', 'where', 'why', 'how'];

function emptyFacets() {
  const f = {};
  for (const k of FACET_KEYS) f[k] = '';
  return f;
}

// Create a rune. `glyph` is its editability type (registered in the glyph
// registry). `content` is glyph-specific and Void Core does not interpret it.
function createRune({ name, id, glyph, facets, tags, content, placement, relations } = {}) {
  if (!glyph || typeof glyph !== 'string') throw new Error('rune requires a glyph');
  return {
    spirit: createSpirit(name, id),
    glyph,
    facets: { ...emptyFacets(), ...(facets || {}) },
    tags: Array.isArray(tags) ? [...tags] : [],
    content: content !== undefined ? content : {},
    placement: placement || null,   // optional explicit position (Codex §3)
    relations: Array.isArray(relations) ? [...relations] : [],
  };
}

// Normalize a loaded-from-JSON rune so older/partial records get all fields.
function hydrateRune(raw) {
  if (!raw || !raw.spirit) throw new Error('cannot hydrate rune without a spirit');
  return {
    spirit: { id: raw.spirit.id, name: raw.spirit.name },
    glyph: raw.glyph || 'text',
    facets: { ...emptyFacets(), ...(raw.facets || {}) },
    tags: Array.isArray(raw.tags) ? raw.tags : [],
    content: raw.content !== undefined ? raw.content : {},
    placement: raw.placement || null,
    relations: Array.isArray(raw.relations) ? raw.relations : [],
  };
}

module.exports = { createRune, hydrateRune, emptyFacets, FACET_KEYS };
