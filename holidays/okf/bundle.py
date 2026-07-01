"""
bundle.py — load and model an OKF Knowledge Bundle.

The shared reader for the OKF engine and the viewer. A *Bundle* is a directory of
markdown files; each non-reserved `.md` is a *Concept* whose Concept ID is its path
minus `.md`. Cross-links between concepts form the graph.

Pure Python, no deps — consumption works on ANY conformant bundle, with or without
Void Core. (Mapping concepts into Void Core runes is a separate, optional step.)
"""
from __future__ import annotations

import glob
import os
import re
from dataclasses import dataclass, field
from typing import Callable, Optional

RESERVED = {"index.md", "log.md"}
_LINK_RE = re.compile(r"\]\((/[^)\s]+\.md|\.{0,2}/?[^)\s]+\.md)\)")


def parse_frontmatter(text: str) -> tuple[dict, str]:
    """Minimal YAML-frontmatter parser for the subset OKF uses (scalars + an inline
    `tags: [a, b]` list). Returns (fields, body)."""
    if not text.startswith("---"):
        return {}, text
    end = text.find("\n---", 3)
    if end == -1:
        return {}, text
    fm_block = text[3:end].strip("\n")
    body = text[end + 4:].lstrip("\n")
    fields: dict = {}
    for line in fm_block.splitlines():
        if not line.strip() or ":" not in line:
            continue
        key, _, val = line.partition(":")
        key, val = key.strip(), val.strip()
        if val.startswith("[") and val.endswith("]"):
            items = [v.strip().strip("'\"") for v in val[1:-1].split(",")]
            fields[key] = [v for v in items if v]
        else:
            fields[key] = val.strip("'\"")
    return fields, body


@dataclass
class Concept:
    id: str                       # path minus .md, bundle-relative, forward slashes
    title: str
    type: str
    section: str                  # top-level dir, or "root"
    tags: list[str] = field(default_factory=list)
    description: str = ""
    resource: str = ""
    timestamp: str = ""
    body: str = ""
    links: list[str] = field(default_factory=list)  # concept IDs this one links to
    path: str = ""                # absolute file path

    @property
    def status(self) -> str:
        for t in self.tags:
            if t.startswith("status:"):
                return t.split(":", 1)[1]
        return "unknown"

    @property
    def audiences(self) -> list[str]:
        return [t.split(":", 1)[1] for t in self.tags if t.startswith("audience:")]

    def matches(self, token: str) -> bool:
        """SPEC §5 atom: a concept is matched by a tag, its id/name, or its type
        (via `type:<value>`)."""
        if token.lower().startswith("type:"):
            return self.type.lower() == token.split(":", 1)[1].lower()
        return token in self.tags or token == self.id or token == self.title


@dataclass
class Bundle:
    dir: str
    concepts: dict[str, Concept]
    edges: list[tuple[str, str]]

    def get(self, cid: str) -> Optional[Concept]:
        return self.concepts.get(cid)

    def degree(self, cid: str) -> int:
        return sum(1 for a, b in self.edges if a == cid or b == cid)

    def backlinks(self, cid: str) -> list[str]:
        return sorted({a for a, b in self.edges if b == cid})

    def query(self, expr: str) -> list[Concept]:
        """Resolve a SPEC §5 tag-filter expression to concepts (empty = all)."""
        pred = compile_filter(expr)
        return [c for c in self.concepts.values() if pred(c)]


def load_bundle(bundle_dir: str) -> Bundle:
    bundle_dir = os.path.abspath(bundle_dir)
    files = [f for f in glob.glob(os.path.join(bundle_dir, "**", "*.md"), recursive=True)
             if os.path.basename(f) not in RESERVED]
    ids = {_cid(f, bundle_dir) for f in files}
    concepts: dict[str, Concept] = {}
    edges: set[tuple[str, str]] = set()
    for f in files:
        text = open(f, encoding="utf-8").read()
        fm, body = parse_frontmatter(text)
        cid = _cid(f, bundle_dir)
        tags = fm.get("tags", []) if isinstance(fm.get("tags"), list) else []
        links = _links(f, body, bundle_dir, ids, cid)
        for tgt in links:
            edges.add((cid, tgt))
        concepts[cid] = Concept(
            id=cid, title=fm.get("title", cid.split("/")[-1]),
            type=fm.get("type", "Concept"),
            section=cid.split("/")[0] if "/" in cid else "root",
            tags=tags, description=fm.get("description", ""),
            resource=fm.get("resource", ""), timestamp=fm.get("timestamp", ""),
            body=body, links=links, path=f,
        )
    return Bundle(dir=bundle_dir, concepts=concepts, edges=sorted(edges))


def _cid(path: str, bundle_dir: str) -> str:
    return os.path.relpath(path, bundle_dir).replace(os.sep, "/")[:-3]


def _links(path: str, body: str, bundle_dir: str, ids: set[str], self_id: str) -> list[str]:
    out: list[str] = []
    here = os.path.dirname(os.path.relpath(path, bundle_dir)).replace(os.sep, "/")
    for m in _LINK_RE.finditer(body):
        target = m.group(1)
        if target.startswith("/"):
            cid = target.lstrip("/")[:-3]
        else:
            cid = os.path.normpath(os.path.join(here, target)).replace(os.sep, "/")[:-3]
        if cid in ids and cid != self_id and cid not in out:
            out.append(cid)
    return out


# ── tag-filter (SPEC §5) compiled to a Python predicate over a Concept ───────────
_KEYWORDS = {"AND", "OR", "NOT"}


def compile_filter(expr: str) -> Callable[[Concept], bool]:
    expr = (expr or "").strip()
    if expr.startswith("@"):
        expr = expr[1:].strip()
    if not expr:
        return lambda c: True
    toks = _tokenize(expr)
    parser = _Parser(toks)
    node = parser.parse_or()
    parser.expect_end()
    return lambda c: node(c)


def _tokenize(s: str) -> list[str]:
    out, i, n = [], 0, len(s)
    while i < n:
        ch = s[i]
        if ch.isspace():
            i += 1
        elif ch in "()":
            out.append(ch); i += 1
        elif ch == "&" and i + 1 < n and s[i + 1] == "&":
            out.append("AND"); i += 2
        elif ch == "|" and i + 1 < n and s[i + 1] == "|":
            out.append("OR"); i += 2
        elif ch == "!":
            out.append("NOT"); i += 1
        else:
            j = i
            while j < n and not s[j].isspace() and s[j] not in "()":
                if s[j] == "!" or (s[j] in "&|" and j + 1 < n and s[j + 1] == s[j]):
                    break
                j += 1
            w = s[i:j]
            out.append(w.upper() if w.upper() in _KEYWORDS else w)
            i = j
    return out


class _Parser:
    def __init__(self, toks): self.toks, self.pos = toks, 0
    def _peek(self): return self.toks[self.pos] if self.pos < len(self.toks) else None
    def _next(self): t = self.toks[self.pos]; self.pos += 1; return t

    def parse_or(self):
        parts = [self.parse_and()]
        while self._peek() == "OR":
            self._next(); parts.append(self.parse_and())
        return parts[0] if len(parts) == 1 else (lambda c, p=parts: any(f(c) for f in p))

    def parse_and(self):
        parts = [self.parse_not()]
        while True:
            t = self._peek()
            if t == "AND":
                self._next(); parts.append(self.parse_not())
            elif t is not None and t not in ("OR", ")"):
                parts.append(self.parse_not())
            else:
                break
        return parts[0] if len(parts) == 1 else (lambda c, p=parts: all(f(c) for f in p))

    def parse_not(self):
        if self._peek() == "NOT":
            self._next(); inner = self.parse_not()
            return lambda c, f=inner: not f(c)
        return self.parse_atom()

    def parse_atom(self):
        t = self._peek()
        if t is None:
            raise ValueError("unexpected end of tag expression")
        if t == "(":
            self._next(); node = self.parse_or()
            if self._peek() != ")":
                raise ValueError("missing ')' in tag expression")
            self._next(); return node
        if t in ("OR", "AND", ")"):
            raise ValueError(f"unexpected token '{t}'")
        tok = self._next()
        return lambda c, tk=tok: c.matches(tk)

    def expect_end(self):
        if self.pos != len(self.toks):
            raise ValueError(f"trailing tokens: {self.toks[self.pos:]}")
