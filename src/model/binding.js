'use strict';
// Binding — a cross-mantle connection. Lives ABOVE mantles (at host level)
// because it references more than one. "When <from rune> <on event>, <to rune>
// <does effect>." This is the home for things like "Click speaks when the reader
// reaches a section." See ../../LEARNINGS.md (the binding layer) and Codex.

const crypto = require('crypto');

// ref string "mantle:rune" or "rune" (caller resolves the default mantle).
function parseRef(ref, defaultMantle) {
  if (!ref) return { mantle: defaultMantle, rune: null };
  const i = ref.indexOf(':');
  if (i === -1) return { mantle: defaultMantle, rune: ref };
  return { mantle: ref.slice(0, i), rune: ref.slice(i + 1) };
}

function createBinding({ name, from, to, on, doEffect, note, id } = {}) {
  if (!from || !to) throw new Error('binding requires from and to');
  return {
    id: id || `bind_${crypto.randomBytes(5).toString('hex')}`,
    name: name || null,
    from: { mantle: from.mantle, rune: from.rune, on: on || from.on || 'reach' },
    to: { mantle: to.mantle, rune: to.rune, do: doEffect || to.do || 'fire' },
    note: note || '',
  };
}

function refString(end, key) { return `${end.mantle}:${end.rune}`; }

module.exports = { createBinding, parseRef, refString };
