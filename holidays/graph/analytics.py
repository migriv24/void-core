"""
analytics.py — pure-Python graph metrics over any Void Core graph.

No dependencies (matching the C core / OKF engine ethos; portable to C/Rust later).
Input is a node-id list + a directed edge list `[(src, dst), ...]`. Centrality treats
the graph as undirected (what "which concept is central?" usually means); PageRank
uses direction (authority); communities use label propagation on the undirected graph.

    metrics = analyze(node_ids, edges)
    metrics["betweenness"]["concepts/rune"]   # -> float
"""
from __future__ import annotations

from collections import deque


def _adjacency(node_ids, edges):
    undirected: dict[str, set] = {n: set() for n in node_ids}
    out_adj: dict[str, set] = {n: set() for n in node_ids}
    in_adj: dict[str, set] = {n: set() for n in node_ids}
    for a, b in edges:
        if a not in undirected or b not in undirected or a == b:
            continue
        undirected[a].add(b)
        undirected[b].add(a)
        out_adj[a].add(b)
        in_adj[b].add(a)
    return undirected, out_adj, in_adj


def degree(node_ids, edges):
    und, out_adj, in_adj = _adjacency(node_ids, edges)
    return ({n: len(und[n]) for n in node_ids},
            {n: len(in_adj[n]) for n in node_ids},
            {n: len(out_adj[n]) for n in node_ids})


def betweenness(node_ids, edges, normalized=True):
    """Brandes' algorithm for unweighted, undirected betweenness centrality."""
    und, _, _ = _adjacency(node_ids, edges)
    bc = {n: 0.0 for n in node_ids}
    for s in node_ids:
        stack, pred = [], {n: [] for n in node_ids}
        sigma = {n: 0.0 for n in node_ids}
        sigma[s] = 1.0
        dist = {n: -1 for n in node_ids}
        dist[s] = 0
        q = deque([s])
        while q:
            v = q.popleft()
            stack.append(v)
            for w in und[v]:
                if dist[w] < 0:
                    dist[w] = dist[v] + 1
                    q.append(w)
                if dist[w] == dist[v] + 1:
                    sigma[w] += sigma[v]
                    pred[w].append(v)
        delta = {n: 0.0 for n in node_ids}
        while stack:
            w = stack.pop()
            for v in pred[w]:
                if sigma[w]:
                    delta[v] += (sigma[v] / sigma[w]) * (1.0 + delta[w])
            if w != s:
                bc[w] += delta[w]
    # undirected: each pair counted twice
    for n in bc:
        bc[n] /= 2.0
    if normalized and len(node_ids) > 2:
        scale = 2.0 / ((len(node_ids) - 1) * (len(node_ids) - 2))
        for n in bc:
            bc[n] *= scale
    return bc


def pagerank(node_ids, edges, damping=0.85, iters=100, tol=1e-9):
    """Directed PageRank via power iteration (authority over the link graph)."""
    _, out_adj, _ = _adjacency(node_ids, edges)
    n = len(node_ids)
    if n == 0:
        return {}
    pr = {v: 1.0 / n for v in node_ids}
    dangling = [v for v in node_ids if not out_adj[v]]
    for _ in range(iters):
        nxt = {v: (1.0 - damping) / n for v in node_ids}
        dangling_mass = damping * sum(pr[v] for v in dangling) / n
        for v in node_ids:
            nxt[v] += dangling_mass
        for v in node_ids:
            if out_adj[v]:
                share = damping * pr[v] / len(out_adj[v])
                for w in out_adj[v]:
                    nxt[w] += share
        if sum(abs(nxt[v] - pr[v]) for v in node_ids) < tol:
            pr = nxt
            break
        pr = nxt
    return pr


def communities(node_ids, edges, iters=50):
    """Label propagation on the undirected graph (deterministic tie-breaking)."""
    und, _, _ = _adjacency(node_ids, edges)
    label = {n: n for n in node_ids}
    order = sorted(node_ids)
    for _ in range(iters):
        changed = False
        for v in order:
            if not und[v]:
                continue
            counts: dict[str, int] = {}
            for w in und[v]:
                counts[label[w]] = counts.get(label[w], 0) + 1
            # most frequent neighbor label; ties -> smallest label string (deterministic)
            top_count = max(counts.values())
            best = min(lab for lab, c in counts.items() if c == top_count)
            if label[v] != best:
                label[v] = best
                changed = True
        if not changed:
            break
    # renumber labels to compact community ids
    uniq = {lab: i for i, lab in enumerate(sorted(set(label.values())))}
    return {n: uniq[label[n]] for n in node_ids}


def components(node_ids, edges):
    """Connected components (undirected)."""
    und, _, _ = _adjacency(node_ids, edges)
    seen, comp, cid = set(), {}, 0
    for s in sorted(node_ids):
        if s in seen:
            continue
        q = deque([s])
        seen.add(s)
        while q:
            v = q.popleft()
            comp[v] = cid
            for w in und[v]:
                if w not in seen:
                    seen.add(w)
                    q.append(w)
        cid += 1
    return comp, cid


def analyze(node_ids, edges):
    node_ids = list(node_ids)
    deg, indeg, outdeg = degree(node_ids, edges)
    bc = betweenness(node_ids, edges)
    pr = pagerank(node_ids, edges)
    comm = communities(node_ids, edges)
    comp, ncomp = components(node_ids, edges)
    return {
        "degree": deg, "in_degree": indeg, "out_degree": outdeg,
        "betweenness": bc, "pagerank": pr, "community": comm, "component": comp,
        "summary": {"nodes": len(node_ids), "edges": len(edges),
                    "components": ncomp, "communities": len(set(comm.values()))},
    }


def top(metric: dict, k: int = 5):
    return sorted(metric.items(), key=lambda kv: kv[1], reverse=True)[:k]
