'use strict';
// Tag system — lives within a mantle (Codex §2). Tags are organizational
// metadata; a rune's spirit-name doubles as a usable tag. Supports a filter
// expression language: AND/OR/NOT (and &&/||/!), parentheses, namespaced tags
// like `group:science`. See ARCHITECTURE.md §3.

function runeTags(rune) {
  // A rune is matched by any of its tags OR its own spirit-name (Codex §2).
  return new Set([...(rune.tags || []), rune.spirit.name]);
}

function addTags(rune, names) {
  for (const n of names) if (n && !rune.tags.includes(n)) rune.tags.push(n);
  return rune.tags;
}
function removeTags(rune, names) {
  rune.tags = rune.tags.filter(t => !names.includes(t));
  return rune.tags;
}

// Count tag usage across a mantle: { tag -> count }.
function tagCounts(mantle) {
  const counts = {};
  for (const r of mantle.runes) {
    for (const t of r.tags || []) counts[t] = (counts[t] || 0) + 1;
  }
  return counts;
}

// ── Filter expression parser → predicate(rune) ──────────────────────
function tokenize(expr) {
  const out = [];
  const re = /\s*(\(|\)|&&|\|\||!|\bAND\b|\bOR\b|\bNOT\b|[^\s()]+)/giy;
  let m;
  let i = 0;
  while (i < expr.length) {
    re.lastIndex = i;
    m = re.exec(expr);
    if (!m) break;
    out.push(m[1]);
    i = re.lastIndex;
  }
  return out;
}

function compile(expr) {
  if (!expr || !expr.trim()) return () => true;
  const toks = tokenize(expr);
  let pos = 0;
  const peek = () => toks[pos];
  const next = () => toks[pos++];
  const isOp = (t, ...ops) => t && ops.some(o => o.toLowerCase() === String(t).toLowerCase());

  function parseOr() {
    let left = parseAnd();
    while (isOp(peek(), 'OR', '||')) { next(); const r = parseAnd(); const l = left; left = (x) => l(x) || r(x); }
    return left;
  }
  function parseAnd() {
    let left = parseNot();
    while (isOp(peek(), 'AND', '&&')) { next(); const r = parseNot(); const l = left; left = (x) => l(x) && r(x); }
    return left;
  }
  function parseNot() {
    if (isOp(peek(), 'NOT', '!')) { next(); const r = parseNot(); return (x) => !r(x); }
    return parseAtom();
  }
  function parseAtom() {
    const t = next();
    if (t === '(') { const e = parseOr(); if (peek() === ')') next(); return e; }
    const tag = t;
    return (rune) => runeTags(rune).has(tag);
  }
  return parseOr();
}

// Filter a mantle's runes by an expression string.
function filter(mantle, expr) {
  const pred = compile(expr);
  return mantle.runes.filter(pred);
}

module.exports = { addTags, removeTags, tagCounts, runeTags, compile, filter };
