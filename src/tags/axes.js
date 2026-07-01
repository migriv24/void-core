'use strict';
// Fundamental tag axes — the small upper ontology every tag hangs off, so that
// independent mantles' (and eventually independent projects') tag systems can
// merge by typed union. A tag is `axis:value` or `namespace:value`; the
// namespace maps to one fundamental axis. See ../../LEARNINGS.md (tag lattice).

// The fixed, orthogonal fundamental axes.
const AXES = {
  where: 'location / site / section',
  what: 'kind of thing',
  who: 'agent / voice',
  when: 'temporal / trigger',
  state: 'status / lifecycle',
  free: 'unclassified',
};

// Map a tag namespace to a fundamental axis. Unknown namespaces -> 'free'.
const NAMESPACE_AXIS = {
  site: 'where', group: 'where', section: 'where', region: 'where', outcome: 'where',
  glyph: 'what', kind: 'what', type: 'what', content: 'what', homepage: 'what', field: 'what',
  who: 'who', voice: 'who', author: 'who', character: 'who', click: 'who',
  when: 'when', trigger: 'when', phase: 'when', time: 'when',
  status: 'state', state: 'state', stage: 'state',
};

// Classify a tag string into { namespace, value, axis }.
function classify(tag) {
  const i = String(tag).indexOf(':');
  if (i === -1) return { namespace: tag, value: null, axis: NAMESPACE_AXIS[tag] || 'free' };
  const ns = tag.slice(0, i);
  return { namespace: ns, value: tag.slice(i + 1), axis: NAMESPACE_AXIS[ns] || 'free' };
}

// Bucket a list of tags by fundamental axis.
function byAxis(tags) {
  const out = {};
  for (const t of tags) {
    const a = classify(t).axis;
    (out[a] = out[a] || []).push(t);
  }
  return out;
}

module.exports = { AXES, NAMESPACE_AXIS, classify, byAxis };
