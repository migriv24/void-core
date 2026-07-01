"""
holiday.py — the graph-analytics compute holiday.

A `kind:compute` Void Core holiday: the core (or the OKF engine) emits a graph; this
holiday computes metrics over it. It owns no data and does no I/O — it transforms a
node/edge set into centrality, communities, and components (see `analytics.py`). Works
on ANY graph; an OKF bundle (a mantle is a graph) is the first consumer.
"""
from __future__ import annotations

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import analytics  # noqa: E402


class GraphAnalyticsHoliday:
    tags = ["kind:compute", "role:graph-analytics", "deterministic:yes", "public:no"]

    def analyze(self, node_ids, edges) -> dict:
        return analytics.analyze(list(node_ids), list(edges))

    def analyze_bundle(self, bundle) -> dict:
        """Analyze an OKF Bundle's concept graph (id list + link edges)."""
        return self.analyze(bundle.concepts.keys(), bundle.edges)

    def analyze_state(self, vc_state: dict, mantle: str | None = None) -> dict:
        """Analyze a Void Core state document's mantle layout graph."""
        mantles = vc_state.get("mantles", [])
        m = next((x for x in mantles if x["name"] == mantle), None) if mantle else (mantles[0] if mantles else None)
        if not m:
            return self.analyze([], [])
        names = [r["spirit"]["name"] for r in m.get("runes", [])]
        edges = [(e["from"], e["to"]) for e in m.get("layout", {}).get("edges", [])]
        return self.analyze(names, edges)

    def describe(self) -> dict:
        return {
            "kind": "compute", "role": "graph-analytics",
            "metrics": ["degree", "in_degree", "out_degree", "betweenness",
                        "pagerank", "community", "component"],
            "tags": self.tags,
        }
