"""
manifest.py — read an application's self-description from its OKF bundle.

Every Void Core app builds on the same `core`, so there's a standard way for an app to
**introduce itself**: a reserved bundle-root concept `app.md` of `type: Manifest`. A tool
that presents the app from outside (a launcher, a registry, FaultSack) reads it instead of
scraping prose. This is the "consuming beats producing" route — pure OKF data, no engine
change, readable from files without running the app.

Two tiers, both optional with sensible defaults:
  - **Identity** (the "who am I" standard): `name`, `id`, `version`, `description`, and
    optionally `authors`, `repo`, `homepage`, `status`.
  - **Representation** (the optional "how do I present myself" layer): a small normative core
    — `palette.<role>` (primary/accent/bg/ink/ok/warn/err), `icon` (a leading-word name),
    `theme` (a name) — plus a free-form bag (any other `theme.*` / representation key), the
    same "known namespaces + a free catch-all" shape as the tag system. The core ships no
    assets; a renderer (a holiday or the host) resolves icon names + concrete palettes.

Frontmatter stays flat (e.g. `palette.primary: "#7c3aed"`) so the existing minimal OKF
frontmatter parser handles it with no change. If `app.md` is absent, the reader falls back to
`index.md` frontmatter, then to scraping `index.md`'s first heading + paragraph, then the
folder name — so any bundle yields *something* (this subsumes the old index.md scrape).

    from manifest import read_manifest
    m = read_manifest("path/to/app/okf")
    m.name, m.description, m.palette.get("primary"), m.icon
"""
from __future__ import annotations

import os
import re
from dataclasses import dataclass, field

from bundle import parse_frontmatter  # holidays/okf/bundle.py (same dir, on sys.path)

# Frontmatter keys consumed by identity/standard handling; everything else (besides
# palette.* and authors) lands in `extra` as the free representation bag.
_IDENTITY = {"name", "id", "version", "description", "status", "repo", "homepage"}
_REP_SCALAR = {"icon", "theme"}
_STD_CONCEPT = {"type", "title", "tags", "timestamp", "resource"}  # generic OKF concept fields


@dataclass
class Manifest:
    name: str
    id: str = ""
    version: str = ""
    description: str = ""
    status: str = ""
    authors: list[str] = field(default_factory=list)
    repo: str = ""
    homepage: str = ""
    icon: str = ""
    theme: str = ""
    palette: dict[str, str] = field(default_factory=dict)   # role -> value
    extra: dict[str, str] = field(default_factory=dict)     # free theme.* / rep keys
    source: str = ""                                         # where it was read from

    def to_dict(self) -> dict:
        return {
            "name": self.name, "id": self.id, "version": self.version,
            "description": self.description, "status": self.status,
            "authors": self.authors, "repo": self.repo, "homepage": self.homepage,
            "representation": {"icon": self.icon, "theme": self.theme,
                               "palette": self.palette, **self.extra},
            "source": self.source,
        }


def _slug(s: str) -> str:
    return re.sub(r"^-+|-+$", "", re.sub(r"[^a-z0-9]+", "-", (s or "").lower())) or "app"


def _scrape_index(bundle_dir: str) -> tuple[str, str]:
    """First `# H1` and first body paragraph of index.md (the legacy 'introduce yourself')."""
    path = os.path.join(bundle_dir, "index.md")
    if not os.path.exists(path):
        return "", ""
    _, body = parse_frontmatter(open(path, encoding="utf-8").read())
    h1 = next((re.sub(r"^#\s+", "", ln).strip()
               for ln in body.splitlines() if ln.startswith("# ")), "")
    para = ""
    for block in re.split(r"\n\s*\n", body):
        block = block.strip()
        if block and not block.startswith("#"):
            para = re.sub(r"\s+", " ", block)
            break
    return h1, para


def read_manifest(bundle_dir: str) -> Manifest:
    """Read an app's manifest from its OKF bundle dir. Always returns a Manifest (defaults
    filled), so callers never have to handle 'missing'."""
    bundle_dir = os.path.abspath(bundle_dir)
    app_md = os.path.join(bundle_dir, "app.md")
    if os.path.exists(app_md):
        fm, _ = parse_frontmatter(open(app_md, encoding="utf-8").read())
        source = "app.md"
    else:
        idx = os.path.join(bundle_dir, "index.md")
        fm = parse_frontmatter(open(idx, encoding="utf-8").read())[0] if os.path.exists(idx) else {}
        source = "index.md frontmatter" if fm else "index.md scrape"

    palette, extra = {}, {}
    for k, v in fm.items():
        if k.startswith("palette."):
            palette[k.split(".", 1)[1]] = v
        elif k in _IDENTITY or k in _REP_SCALAR or k in _STD_CONCEPT or k == "authors":
            continue
        else:
            extra[k] = v  # free representation bag (theme.*, custom keys)

    h1, para = _scrape_index(bundle_dir)
    # name: explicit > index H1/title > folder (the app dir, not a bare "okf")
    folder = os.path.basename(bundle_dir)
    if folder.lower() == "okf":
        folder = os.path.basename(os.path.dirname(bundle_dir)) or folder
    name = fm.get("name") or fm.get("title") or h1 or folder
    authors = fm.get("authors", [])
    if isinstance(authors, str):
        authors = [authors] if authors else []

    return Manifest(
        name=name,
        id=fm.get("id") or _slug(name),
        version=fm.get("version", ""),
        description=fm.get("description") or para,
        status=fm.get("status", ""),
        authors=authors,
        repo=fm.get("repo", ""), homepage=fm.get("homepage", ""),
        icon=fm.get("icon", ""), theme=fm.get("theme", ""),
        palette=palette, extra=extra, source=source,
    )
