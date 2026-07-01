"""
validate.py — OKF conformance + drift detection.

OKF doesn't keep itself accurate; this makes drift *detectable*. Three layers:

  errors    — conformance violations (OKF §9: every concept needs a non-empty `type`).
  warnings  — honesty/freshness: a `status:current` concept with no backing
              `resource:`; a `resource:` that no longer exists (dead); a `resource:`
              whose file changed on a later date than the concept's `timestamp` (stale).
  info      — broken *intra-bundle* links. OKF says to tolerate these ("not-yet-written
              knowledge"), so they are informational, never errors.

Freshness uses filesystem mtime at day granularity (this repo has no git history yet),
so same-day edits don't raise false alarms.
"""
from __future__ import annotations

import os
import re
from dataclasses import dataclass, field
from datetime import date, datetime

from bundle import Bundle, RESERVED, parse_frontmatter

_BODY_LINK_RE = re.compile(r"\]\((/[^)\s]+\.md)\)")
# Documentation-type concepts describe a standard, a mapping, an app's identity, or design
# rationale — not code — so the "status:current needs a code `resource:`" honesty rule does
# not apply to them.
_DOC_TYPES = {"dictionary", "reference", "roadmap", "manifest", "design"}


@dataclass
class Report:
    bundle: str
    concepts: int = 0
    errors: list[tuple[str, str]] = field(default_factory=list)
    warnings: list[tuple[str, str]] = field(default_factory=list)
    info: list[tuple[str, str]] = field(default_factory=list)
    freshness: dict[str, str] = field(default_factory=dict)

    @property
    def ok(self) -> bool:
        return not self.errors

    def render(self) -> str:
        lines = [f"OKF validate: {self.bundle}",
                 f"  concepts: {self.concepts}  errors: {len(self.errors)}  "
                 f"warnings: {len(self.warnings)}  info: {len(self.info)}"]
        for label, items in (("ERROR", self.errors), ("WARN", self.warnings), ("INFO", self.info)):
            for cid, msg in items:
                lines.append(f"  [{label}] {cid}: {msg}")
        counts: dict[str, int] = {}
        for v in self.freshness.values():
            counts[v] = counts.get(v, 0) + 1
        if counts:
            lines.append("  freshness: " + ", ".join(f"{k}={v}" for k, v in sorted(counts.items())))
        lines.append("  RESULT: " + ("CONFORMANT" if self.ok else "NON-CONFORMANT"))
        return "\n".join(lines)


def _to_date(ts: str):
    try:
        return datetime.fromisoformat(ts.replace("Z", "+00:00")).date()
    except (ValueError, AttributeError):
        return None


def validate(bundle: Bundle) -> Report:
    repo_root = os.path.dirname(bundle.dir)
    rep = Report(bundle=bundle.dir, concepts=len(bundle.concepts))
    valid_ids = set(bundle.concepts)

    for cid, c in sorted(bundle.concepts.items()):
        # conformance: re-read so a defaulted `type` can't mask a missing field
        raw, _ = parse_frontmatter(open(c.path, encoding="utf-8").read())
        if not str(raw.get("type", "")).strip():
            rep.errors.append((cid, "missing required `type` (OKF §9)"))

        # honesty + freshness on `resource`
        res = c.resource.strip()
        if res.startswith(("http://", "https://")):
            rep.freshness[cid] = "external"
        elif res:
            target = os.path.join(repo_root, res.replace("/", os.sep))
            if not os.path.exists(target):
                rep.freshness[cid] = "dead"
                rep.warnings.append((cid, f"resource not found: {res}"))
            else:
                fdate = date.fromtimestamp(os.path.getmtime(target))
                ddate = _to_date(c.timestamp)
                if ddate and fdate > ddate:
                    rep.freshness[cid] = "stale"
                    rep.warnings.append(
                        (cid, f"resource {res} changed {fdate} after doc timestamp {ddate}"))
                else:
                    rep.freshness[cid] = "fresh"
        else:
            rep.freshness[cid] = "none"
            if c.status == "current" and c.type.lower() not in _DOC_TYPES:
                rep.warnings.append(
                    (cid, "status:current but no `resource:` backing it (honesty rule)"))

        # broken intra-bundle links (tolerated by OKF — informational)
        for m in _BODY_LINK_RE.finditer(c.body):
            tgt = m.group(1).lstrip("/")
            base = os.path.basename(tgt)
            tid = tgt[:-3]
            if base in RESERVED:
                continue
            if tid not in valid_ids:
                rep.info.append((cid, f"link to missing concept /{tgt} (tolerated)"))

    return rep


def suggested_confidence(rep: Report, cid: str) -> str:
    """What `confidence:` the validator would stamp (not written unless --fix)."""
    f = rep.freshness.get(cid, "none")
    if f in ("dead", "stale"):
        return "stale"
    if f in ("fresh", "external"):
        return "verified"
    return "asserted"
