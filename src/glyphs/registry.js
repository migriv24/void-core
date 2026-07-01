'use strict';
// Glyph registry — binds a rune type to (a) how it's edited and (b) how it's
// described textually for the CLI/agent. See ARCHITECTURE.md §2.2.
// Twins register website-specific glyphs (bubble, dialogueLine, characterConfig…)
// on top of these built-ins.

function createRegistry() {
  const glyphs = new Map();

  function register(def) {
    if (!def || !def.glyph) throw new Error('glyph def requires a `glyph` name');
    glyphs.set(def.glyph, {
      glyph: def.glyph,
      label: def.label || def.glyph,
      editor: def.editor || 'text',          // which GUI editor / CLI prompt
      fields: def.fields || ['value'],        // content field names
      schema: def.schema || null,
      // describe(rune) -> short human/agent summary of its content
      describe: typeof def.describe === 'function'
        ? def.describe
        : (rune) => JSON.stringify(rune.content),
      // newContent() -> default content payload for a freshly minted rune
      newContent: typeof def.newContent === 'function' ? def.newContent : () => ({}),
    });
    return glyphs.get(def.glyph);
  }

  function get(name) { return glyphs.get(name) || null; }
  function has(name) { return glyphs.has(name); }
  function list() { return [...glyphs.values()]; }

  return { register, get, has, list };
}

// ── Built-in glyphs (Codex §2 minimum set) ──────────────────────────
function registerBuiltins(reg) {
  reg.register({
    glyph: 'text', label: 'Text block', editor: 'text', fields: ['value'],
    newContent: () => ({ value: '' }),
    describe: (r) => String(r.content.value ?? ''),
  });
  reg.register({
    glyph: 'richtext', label: 'Rich text', editor: 'richtext', fields: ['html'],
    newContent: () => ({ html: '' }),
    describe: (r) => String(r.content.html ?? '').replace(/<[^>]+>/g, ' ').trim(),
  });
  reg.register({
    glyph: 'image', label: 'Image', editor: 'image', fields: ['src', 'alt'],
    newContent: () => ({ src: '', alt: '' }),
    describe: (r) => `image: ${r.content.src || '(none)'}${r.content.alt ? ` — ${r.content.alt}` : ''}`,
  });
  reg.register({
    glyph: 'imageList', label: 'Image list', editor: 'imageList', fields: ['images'],
    newContent: () => ({ images: [] }),
    describe: (r) => `${(r.content.images || []).length} image(s)`,
  });
  reg.register({
    glyph: 'color', label: 'Color', editor: 'color', fields: ['value'],
    newContent: () => ({ value: '#000000' }),
    describe: (r) => `color ${r.content.value || '(unset)'}`,
  });
  reg.register({
    glyph: 'link', label: 'Link', editor: 'link', fields: ['url', 'label'],
    newContent: () => ({ url: '', label: '' }),
    describe: (r) => `${r.content.label || 'link'} -> ${r.content.url || '(none)'}`,
  });
  reg.register({
    glyph: 'group', label: 'Group', editor: 'group', fields: ['children'],
    newContent: () => ({ children: [] }),
    describe: (r) => `group of ${(r.content.children || []).length}`,
  });
  return reg;
}

module.exports = { createRegistry, registerBuiltins };
