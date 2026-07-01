# Graph-analytics holiday

A Void Core **compute holiday** (`kind:compute`): the core (or the OKF engine) emits a
graph; this holiday computes metrics over it. It owns no data and does no I/O — it
transforms a node/edge set into centrality, communities, and components. Pure Python,
**no dependencies** (matching the C core / OKF engine ethos; portable to C/Rust later).

Why a holiday and not a core feature: heavy algorithms stay out of the minimal core;
any graph capability the core gains, every mantle-shaped thing (incl. an OKF bundle)
inherits — *a bundle is a mantle is a graph*.

## Files

| file | what |
|---|---|
| `analytics.py` | the algorithms: degree, in/out-degree, betweenness (Brandes), PageRank (power iteration), label-propagation communities, connected components |
| `holiday.py` | `GraphAnalyticsHoliday` — analyze a node/edge set, an OKF `Bundle`, or a Void Core state-document mantle (`layout.edges`) |

## Metrics

- **betweenness** — how often a node bridges shortest paths ("which concept routes the most"). Undirected.
- **pagerank** — authority over the directed link graph ("which is most linked-to").
- **community** — label propagation (deterministic); collapses to one on small dense graphs.
- **degree / in / out**, **components**.

Validated against known-value graphs (path, star) in the self-test.

## Use

```python
from holiday import GraphAnalyticsHoliday
m = GraphAnalyticsHoliday().analyze_bundle(bundle)   # or .analyze(node_ids, edges)
m["betweenness"]["concepts/rune"]
```

Over the dev OKF bundle: `python holidays/okf analyze`. The viewer (`okf serve`) uses
betweenness for its "most central" list and shows each concept's betweenness + community.

## Planned

A modularity-based community method (Louvain) for richer clustering on dense graphs,
and exposing analytics as core dispatcher verbs. The optional NLP/embedding layer
(semantic link-suggestion, context optimization) is a separate, later holiday.
