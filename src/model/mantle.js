'use strict';
// Mantle — a group of runes over a domain, plus the rules between them.
// See ARCHITECTURE.md §2.3. Carries (a) a layout relationship graph and
// (b) an event/rule set — both stored from day one, consumed by future modules.

const crypto = require('crypto');
const { hydrateRune } = require('./rune');

function createMantle({ name, domain, runes, tags, layout, rules, id } = {}) {
  if (!name) throw new Error('mantle requires a name');
  return {
    id: id || `mantle_${crypto.randomBytes(6).toString('hex')}`,
    name,
    domain: domain || null,                 // domain name this mantle sits on
    runes: Array.isArray(runes) ? runes.map(hydrateRune) : [],
    tags: tags || {},                        // tag definitions (see tags/)
    layout: layout || { edges: [] },         // relationship graph (Codex §3)
    rules: Array.isArray(rules) ? rules : [], // event/behavior rules (Codex §3)
  };
}

// ── Rune lookup & mutation ──────────────────────────────────────────
function findRune(mantle, ref) {
  if (!ref) return null;
  return mantle.runes.find(r => r.spirit.name === ref || r.spirit.id === ref) || null;
}

function addRune(mantle, rune) {
  if (findRune(mantle, rune.spirit.name)) {
    throw new Error(`a rune named "${rune.spirit.name}" already exists in this mantle`);
  }
  mantle.runes.push(rune);
  return rune;
}

function removeRune(mantle, ref) {
  const before = mantle.runes.length;
  const target = findRune(mantle, ref);
  if (!target) return false;
  mantle.runes = mantle.runes.filter(r => r !== target);
  // also drop layout edges that referenced it
  mantle.layout.edges = (mantle.layout.edges || []).filter(
    e => e.from !== target.spirit.name && e.to !== target.spirit.name
  );
  return mantle.runes.length < before;
}

function renameRune(mantle, ref, newName) {
  const r = findRune(mantle, ref);
  if (!r) throw new Error(`no rune "${ref}"`);
  if (findRune(mantle, newName)) throw new Error(`name "${newName}" is taken`);
  const oldName = r.spirit.name;
  r.spirit.name = newName;                 // spirit.id stays frozen
  // repoint references that used the old spirit-name (tags + layout edges)
  for (const other of mantle.runes) {
    other.tags = other.tags.map(t => (t === oldName ? newName : t));
  }
  for (const e of mantle.layout.edges || []) {
    if (e.from === oldName) e.from = newName;
    if (e.to === oldName) e.to = newName;
  }
  return r;
}

module.exports = { createMantle, findRune, addRune, removeRune, renameRune };
