"""
tag_filter.py — compile a Void Core tag-filter expression (SPEC §5) to Cypher.

The core's filter grammar:

    expr := or
    or   := and ( ("OR"  | "||") and )*
    and  := not ( ("AND" | "&&") not )*
    not  := ("NOT" | "!") not | atom
    atom := "(" or ")" | TAG

Adjacent atoms are an implicit AND. Operators are case-insensitive. An empty
expression matches everything.

We compile against a query shape where each rune has already been folded to its
collected tag list plus its scalar props, i.e. the Cypher pipeline does:

    MATCH (r:Rune) OPTIONAL MATCH (r)-[:TAGGED]->(t:Tag)
    WITH r, collect(t.name) AS tags
    WHERE <predicate>

A TAG atom `X` matches a rune when (SPEC §5: a rune is matched by any of its tags,
its own `spirit.name`, or its `glyph:<name>`):

    * X == "glyph:<g>"  ->  r.glyph = "<g>"
    * otherwise         ->  (X IN tags OR r.name = X)

`translate(expr)` returns ``(predicate_str, params_dict)``; an empty predicate
string means "no WHERE clause" (match all).
"""
from __future__ import annotations

from typing import Optional

_KEYWORDS = {"AND", "OR", "NOT"}


def translate(expr: str) -> tuple[str, dict]:
    expr = (expr or "").strip()
    if expr.startswith("@"):  # tolerate the core's `@<filter>` targeting form
        expr = expr[1:].strip()
    if not expr:
        return "", {}
    tokens = _tokenize(expr)
    parser = _Parser(tokens)
    node = parser.parse_or()
    parser.expect_end()
    params: dict = {}
    pred = node.to_cypher(params)
    return pred, params


# ── tokenizer ──────────────────────────────────────────────────────────────────
def _tokenize(s: str) -> list[str]:
    out: list[str] = []
    i, n = 0, len(s)
    while i < n:
        c = s[i]
        if c.isspace():
            i += 1
        elif c in "()":
            out.append(c)
            i += 1
        elif c == "&" and i + 1 < n and s[i + 1] == "&":
            out.append("AND"); i += 2
        elif c == "|" and i + 1 < n and s[i + 1] == "|":
            out.append("OR"); i += 2
        elif c == "!":
            out.append("NOT"); i += 1
        else:
            j = i
            while j < n and not s[j].isspace() and s[j] not in "()":
                # `!` and the start of `&&`/`||` end a bare word too
                if s[j] == "!" or (s[j] in "&|" and j + 1 < n and s[j + 1] == s[j]):
                    break
                j += 1
            word = s[i:j]
            out.append(word.upper() if word.upper() in _KEYWORDS else word)
            i = j
    return out


# ── AST ────────────────────────────────────────────────────────────────────────
class _Node:
    def to_cypher(self, params: dict) -> str:  # pragma: no cover - interface
        raise NotImplementedError


class _And(_Node):
    def __init__(self, parts): self.parts = parts
    def to_cypher(self, params):
        return "(" + " AND ".join(p.to_cypher(params) for p in self.parts) + ")"


class _Or(_Node):
    def __init__(self, parts): self.parts = parts
    def to_cypher(self, params):
        return "(" + " OR ".join(p.to_cypher(params) for p in self.parts) + ")"


class _Not(_Node):
    def __init__(self, inner): self.inner = inner
    def to_cypher(self, params):
        return "(NOT " + self.inner.to_cypher(params) + ")"


class _Tag(_Node):
    def __init__(self, tag): self.tag = tag
    def to_cypher(self, params):
        key = f"p{len(params)}"
        if self.tag.lower().startswith("glyph:"):
            params[key] = self.tag.split(":", 1)[1]
            return f"r.glyph = ${key}"
        params[key] = self.tag
        return f"(${key} IN tags OR r.name = ${key})"


# ── recursive-descent parser ─────────────────────────────────────────────────────
class _Parser:
    def __init__(self, tokens: list[str]):
        self.toks = tokens
        self.pos = 0

    def _peek(self) -> Optional[str]:
        return self.toks[self.pos] if self.pos < len(self.toks) else None

    def _next(self) -> str:
        t = self.toks[self.pos]
        self.pos += 1
        return t

    def parse_or(self) -> _Node:
        parts = [self.parse_and()]
        while self._peek() == "OR":
            self._next()
            parts.append(self.parse_and())
        return parts[0] if len(parts) == 1 else _Or(parts)

    def parse_and(self) -> _Node:
        parts = [self.parse_not()]
        while True:
            t = self._peek()
            if t == "AND":
                self._next()
                parts.append(self.parse_not())
            elif t is not None and t not in ("OR", ")"):
                # implicit AND: another atom/NOT/( directly follows
                parts.append(self.parse_not())
            else:
                break
        return parts[0] if len(parts) == 1 else _And(parts)

    def parse_not(self) -> _Node:
        if self._peek() == "NOT":
            self._next()
            return _Not(self.parse_not())
        return self.parse_atom()

    def parse_atom(self) -> _Node:
        t = self._peek()
        if t is None:
            raise ValueError("unexpected end of tag expression")
        if t == "(":
            self._next()
            node = self.parse_or()
            if self._peek() != ")":
                raise ValueError("missing ')' in tag expression")
            self._next()
            return node
        if t in ("OR", "AND", ")"):
            raise ValueError(f"unexpected token '{t}' in tag expression")
        return _Tag(self._next())

    def expect_end(self) -> None:
        if self.pos != len(self.toks):
            raise ValueError(f"trailing tokens in tag expression: {self.toks[self.pos:]}")
