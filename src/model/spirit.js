'use strict';
// Spirit — a rune's identity: a frozen random real-ID + a human-readable name.
// See ARCHITECTURE.md §2.1.

const crypto = require('crypto');

// Mint a real ID: a prefix + random hex. Stable, unique, never reused.
function mintId(prefix = 'rune') {
  return `${prefix}_${crypto.randomBytes(6).toString('hex')}`;
}

// Create a spirit. If no id is given, one is minted. `name` is the human handle
// (doubles as a tag), must be unique within a mantle (enforced by the mantle).
function createSpirit(name, id) {
  if (!name || typeof name !== 'string') {
    throw new Error('spirit requires a name');
  }
  return { id: id || mintId(), name };
}

module.exports = { mintId, createSpirit };
