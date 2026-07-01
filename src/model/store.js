'use strict';
// Store — persistence of the manager's *working state* (the Save Progress target
// at the model level). One JSON file holds all domains, mantles, scripts, config.
// NOTE: this is the abstract working state. Writing edits back into the website's
// real source files is done by site-sync adapters (see dispatch `save`), which a
// twin provides. See ARCHITECTURE.md §2 / §10.

const fs = require('fs');
const path = require('path');
const { createMantle } = require('./mantle');

function emptyState() {
  return {
    version: 1,
    domains: {},          // name -> domain object
    mantles: [],          // array of mantle objects
    bindings: [],         // cross-mantle bindings (host-level connections)
    scripts: {},          // name -> voidscript source string
    config: {},           // manager config (ports, paths, defaults)
    active: { mantle: null, domain: null },
  };
}

function hydrateState(raw) {
  const s = emptyState();
  if (!raw) return s;
  Object.assign(s, {
    version: raw.version || 1,
    domains: raw.domains || {},
    bindings: raw.bindings || [],
    scripts: raw.scripts || {},
    config: raw.config || {},
    active: raw.active || { mantle: null, domain: null },
  });
  s.mantles = (raw.mantles || []).map(createMantle);
  if (raw._baseline) s._baseline = raw._baseline;   // last Save Progress snapshot
  return s;
}

function createStore(file) {
  return {
    file,
    load() {
      try {
        const raw = JSON.parse(fs.readFileSync(file, 'utf8'));
        return hydrateState(raw);
      } catch {
        return emptyState();
      }
    },
    save(state) {
      fs.mkdirSync(path.dirname(file), { recursive: true });
      fs.writeFileSync(file, JSON.stringify(state, null, 2), 'utf8');
      return file;
    },
  };
}

module.exports = { createStore, emptyState, hydrateState };
