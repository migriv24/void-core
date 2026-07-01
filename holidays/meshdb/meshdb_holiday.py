"""
meshdb_holiday.py — a Void Core *holiday* backed by a local MeshDB graph BaaS.

A Bolt client + lifecycle manager wrapping a local, offline MeshDB (a
Cypher-compliant graph DB) as the default data holiday: insert/get/query/update/
delete over runes, with tag expressions resolved to Cypher and tags stored as
first-class graph nodes. Construct via `MeshDBHoliday.local_baas(root_dir=...)`
(spawns its own server) or `.connect("bolt://…")` (attach to a running one).

Full rationale, the graph model, and why a graph BaaS is the default:
see okf/components/meshdb-holiday.md and the Holiday concept (okf/concepts/holiday.md).
"""
from __future__ import annotations

import json
import os
import socket
import subprocess
import time
from dataclasses import dataclass, field
from typing import Any, Iterable, Optional

try:
    from neo4j import GraphDatabase
except ImportError as e:  # pragma: no cover
    raise ImportError("the MeshDB holiday needs the Bolt driver: pip install neo4j") from e

from tag_filter import translate as _translate_filter  # local module, same dir


# ── the six Void Core facets, stored as f_<name> to dodge Cypher keywords ──────
_FACETS = ("who", "what", "when", "where", "why", "how")


def _rune_to_props(rune: dict) -> dict:
    """Flatten a Void Core rune (SPEC §3.2) into node properties.

    `content` is opaque to the core, so it crosses as a JSON string. Facets land
    as `f_who`..`f_how` scalar strings. `tags` and `spirit` are handled as edges /
    identity by the caller, not as properties."""
    spirit = rune.get("spirit", {})
    facets = rune.get("facets", {}) or {}
    props = {
        "id": spirit["id"],
        "name": spirit.get("name", spirit["id"]),
        "glyph": rune.get("glyph", "text"),
        "content": json.dumps(rune.get("content", {}), separators=(",", ":")),
    }
    for f in _FACETS:
        props[f"f_{f}"] = facets.get(f, "")
    return props


def _props_to_rune(props: dict, tags: list[str]) -> dict:
    """Rebuild a Void Core rune from node properties + its tag edges."""
    try:
        content = json.loads(props.get("content") or "{}")
    except (json.JSONDecodeError, TypeError):
        content = {}
    return {
        "spirit": {"id": props["id"], "name": props.get("name", props["id"])},
        "glyph": props.get("glyph", "text"),
        "facets": {f: props.get(f"f_{f}", "") for f in _FACETS},
        "tags": sorted(tags),
        "content": content,
        "placement": None,
        "relations": [],
    }


@dataclass
class MeshDBHoliday:
    """A Void Core data-holiday over a MeshDB graph store (Bolt/Cypher).

    Hold a driver + (optionally) the child `meshdb-server` process it manages.
    Construct via :meth:`local_baas` or :meth:`connect`, not directly."""

    driver: Any
    database: str = "neo4j"
    _proc: Optional[subprocess.Popen] = None
    _log_path: Optional[str] = None
    uri: str = ""
    # Holiday tags (SPEC §10 / the synthesis): how a registry routes to this node.
    tags: list[str] = field(default_factory=lambda: [
        "kind:data", "protocol:bolt", "protocol:cypher",
        "consistency:strong", "replicated:no", "public:no", "role:baas",
    ])

    # ── constructors ──────────────────────────────────────────────────────────
    @classmethod
    def connect(
        cls,
        uri: str = "bolt://127.0.0.1:7687",
        auth: Optional[tuple[str, str]] = None,
        *,
        database: str = "neo4j",
        tags: Optional[list[str]] = None,
    ) -> "MeshDBHoliday":
        """Attach to an already-running meshdb-server over Bolt."""
        # MeshDB single-node with no [bolt_auth] is accept-any; the driver still
        # wants *a* principal, so default to throwaway creds.
        driver = GraphDatabase.driver(uri, auth=auth or ("neo4j", "neo4j"))
        driver.verify_connectivity()
        h = cls(driver=driver, database=database, uri=uri)
        if tags is not None:
            h.tags = tags
        return h

    @classmethod
    def local_baas(
        cls,
        root_dir: str,
        *,
        bolt_port: int = 7687,
        grpc_port: int = 7601,
        server_bin: Optional[str] = None,
        reuse: bool = True,
        startup_timeout: float = 30.0,
    ) -> "MeshDBHoliday":
        """Construct a *mini local BaaS*: ensure a single-node meshdb-server is up
        (its own data dir under ``root_dir``, no auth, Bolt on ``bolt_port``) and
        return a holiday bound to it.

        If ``reuse`` and the Bolt port is already serving, attach instead of
        spawning a second server (idempotent — safe to call on every app start)."""
        uri = f"bolt://127.0.0.1:{bolt_port}"
        if reuse and _port_open("127.0.0.1", bolt_port):
            return cls.connect(uri)

        os.makedirs(root_dir, exist_ok=True)
        data_dir = os.path.join(root_dir, "mesh-data")
        cfg_path = os.path.join(root_dir, "mesh.toml")
        log_path = os.path.join(root_dir, "meshdb-server.log")
        _write_config(cfg_path, data_dir, grpc_port, bolt_port)

        binary = server_bin or _find_server_bin()
        logf = open(log_path, "w", encoding="utf-8")
        env = dict(os.environ, RUST_LOG=os.environ.get("RUST_LOG", "info"))
        proc = subprocess.Popen(
            [binary, "--config", cfg_path],
            stdout=logf, stderr=subprocess.STDOUT, env=env,
        )
        try:
            holiday = cls._await_bolt(uri, proc, log_path, startup_timeout)
        except Exception:
            proc.terminate()
            logf.close()
            raise
        holiday._proc = proc
        holiday._log_path = log_path
        return holiday

    @classmethod
    def _await_bolt(cls, uri: str, proc, log_path: str, timeout: float) -> "MeshDBHoliday":
        deadline = time.time() + timeout
        last_err: Optional[Exception] = None
        while time.time() < deadline:
            if proc.poll() is not None:
                tail = _tail(log_path)
                raise RuntimeError(
                    f"meshdb-server exited early (code {proc.returncode}).\n{tail}")
            try:
                return cls.connect(uri)
            except Exception as e:  # driver not ready yet
                last_err = e
                time.sleep(0.25)
        raise TimeoutError(f"meshdb-server Bolt port not ready in {timeout}s: {last_err}")

    # ── the holiday interface (SPEC §10.1 / the synthesis trait) ───────────────
    def insert(self, rune: dict, *, mantle: str = "default") -> str:
        """Materialize a Void Core rune into the graph; return its ref (spirit.id).

        Idempotent on `id` (MERGE), so re-inserting an edited rune updates it.
        Tag edges are fully rebuilt to mirror the rune's current `tags`."""
        props = _rune_to_props(rune)
        tags = list(rune.get("tags", []))
        cypher = (
            "MERGE (m:Mantle {name:$mantle}) "
            "MERGE (r:Rune {id:$props.id}) SET r = $props "
            "MERGE (r)-[:IN_MANTLE]->(m) "
            "WITH r "
            "OPTIONAL MATCH (r)-[old:TAGGED]->(:Tag) DELETE old "
            "WITH r "
            "UNWIND $tags AS tname "
            "MERGE (t:Tag {name:tname}) MERGE (r)-[:TAGGED]->(t)"
        )
        self._run(cypher, mantle=mantle, props=props, tags=tags or [])
        return props["id"]

    def get(self, ref: str, *, mantle: Optional[str] = None) -> Optional[dict]:
        """Fetch one rune by spirit.name or spirit.id; None if absent."""
        scope = "-[:IN_MANTLE]->(:Mantle {name:$mantle})" if mantle else ""
        cypher = (
            f"MATCH (r:Rune){scope} WHERE r.id = $ref OR r.name = $ref "
            "OPTIONAL MATCH (r)-[:TAGGED]->(t:Tag) "
            "RETURN r AS r, collect(t.name) AS tags LIMIT 1"
        )
        rows = self._run(cypher, ref=ref, mantle=mantle)
        if not rows:
            return None
        return _props_to_rune(dict(rows[0]["r"]), rows[0]["tags"])

    def query(self, where: str = "", *, mantle: Optional[str] = None,
              limit: Optional[int] = None) -> list[dict]:
        """Resolve a Void Core tag-filter expression (SPEC §5) to runes.

        `where` is the same `@`-style grammar the core uses (AND/OR/NOT, parens,
        implicit-AND, `glyph:` and bare-name atoms); it is compiled to a Cypher
        predicate over each rune's collected tag set + name + glyph."""
        predicate, params = _translate_filter(where)
        scope = "-[:IN_MANTLE]->(:Mantle {name:$mantle})" if mantle else ""
        clause = f"WHERE {predicate} " if predicate else ""
        cap = f"LIMIT {int(limit)}" if limit else ""
        cypher = (
            f"MATCH (r:Rune){scope} "
            "OPTIONAL MATCH (r)-[:TAGGED]->(t:Tag) "
            "WITH r, collect(t.name) AS tags "
            f"{clause}"
            f"RETURN r AS r, tags ORDER BY r.name {cap}"
        )
        rows = self._run(cypher, mantle=mantle, **params)
        return [_props_to_rune(dict(row["r"]), row["tags"]) for row in rows]

    def update(self, ref: str, patch: dict, *, mantle: Optional[str] = None) -> bool:
        """Set content fields on a rune (patch is merged into `content`)."""
        cur = self.get(ref, mantle=mantle)
        if cur is None:
            return False
        cur["content"].update(patch)
        self.insert(cur, mantle=mantle or "default")
        return True

    def delete(self, ref: str, *, mantle: Optional[str] = None) -> bool:
        """Remove a rune and its edges (orphan :Tag nodes are left for reuse)."""
        scope = "-[:IN_MANTLE]->(:Mantle {name:$mantle})" if mantle else ""
        cypher = (
            f"MATCH (r:Rune){scope} WHERE r.id = $ref OR r.name = $ref "
            "WITH r LIMIT 1 DETACH DELETE r RETURN count(*) AS n"
        )
        rows = self._run(cypher, ref=ref, mantle=mantle)
        return bool(rows and rows[0]["n"])

    def describe(self) -> dict:
        """Holiday introspection: capabilities, kind, status, and live counts."""
        status, counts = "online", {}
        try:
            rows = self._run(
                "MATCH (r:Rune) WITH count(r) AS runes "
                "OPTIONAL MATCH (t:Tag) WITH runes, count(t) AS tags "
                "OPTIONAL MATCH (m:Mantle) RETURN runes, tags, count(m) AS mantles")
            if rows:
                counts = {k: rows[0][k] for k in ("runes", "tags", "mantles")}
        except Exception as e:
            status = f"error: {e}"
        return {
            "kind": "data",
            "backend": "meshdb",
            "protocol": "bolt/cypher",
            "uri": self.uri,
            "managed": self._proc is not None,
            "status": status,
            "tags": self.tags,
            "capabilities": self.capabilities(),
            "counts": counts,
        }

    def capabilities(self) -> dict:
        return {"query": True, "get": True, "insert": True, "update": True,
                "delete": True, "transactions": True, "graph": True}

    # ── lifecycle ──────────────────────────────────────────────────────────────
    def wipe(self) -> None:
        """Delete every node (test helper for a clean BaaS)."""
        self._run("MATCH (n) DETACH DELETE n")

    def _run(self, cypher: str, **params) -> list:
        with self.driver.session(database=self.database) as s:
            return list(s.run(cypher, **params))

    def close(self, *, stop_server: bool = True) -> None:
        try:
            self.driver.close()
        finally:
            if stop_server and self._proc is not None:
                self._proc.terminate()
                try:
                    self._proc.wait(timeout=10)
                except subprocess.TimeoutExpired:
                    self._proc.kill()
                self._proc = None

    def __enter__(self) -> "MeshDBHoliday":
        return self

    def __exit__(self, *exc) -> None:
        self.close()


# ── helpers ────────────────────────────────────────────────────────────────────
def _port_open(host: str, port: int, timeout: float = 0.4) -> bool:
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
        sock.settimeout(timeout)
        return sock.connect_ex((host, port)) == 0


def _write_config(path: str, data_dir: str, grpc_port: int, bolt_port: int) -> None:
    # Single-node: empty `peers` + unset `mode` => ClusterMode::Single (local
    # RocksDB). No [bolt_auth] => accept-any, fine for a loopback dev BaaS.
    toml = (
        "self_id = 1\n"
        f'listen_address = "127.0.0.1:{grpc_port}"\n'
        f'data_dir = "{data_dir.replace(os.sep, "/")}"\n'
        f'bolt_address = "127.0.0.1:{bolt_port}"\n'
    )
    with open(path, "w", encoding="utf-8") as f:
        f.write(toml)


def _find_server_bin() -> str:
    here = os.path.dirname(os.path.abspath(__file__))
    repo_parent = os.path.abspath(os.path.join(here, "..", "..", ".."))
    exe = "meshdb-server.exe" if os.name == "nt" else "meshdb-server"
    candidates = [
        os.path.join(repo_parent, "meshdb", "target", "release", exe),
        os.path.join(repo_parent, "meshdb", "target", "debug", exe),
    ]
    for c in candidates:
        if os.path.exists(c):
            return c
    found = _which(exe)
    if found:
        return found
    raise FileNotFoundError(
        "meshdb-server binary not found. Build it:\n"
        "  cargo build -p meshdb-server   (in the meshdb checkout)\n"
        f"looked in: {candidates}")


def _which(name: str) -> Optional[str]:
    for d in os.environ.get("PATH", "").split(os.pathsep):
        cand = os.path.join(d, name)
        if os.path.exists(cand):
            return cand
    return None


def _tail(path: str, n: int = 20) -> str:
    try:
        with open(path, encoding="utf-8", errors="replace") as f:
            return "".join(f.readlines()[-n:])
    except OSError:
        return "(no server log)"
