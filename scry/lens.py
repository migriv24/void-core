"""
scry/lens.py — a Lens: a bidirectional Scry projection that inherits the round-trip law.

A one-way [scry](projection.py) projects state → view. A **Lens** is the two-way case: a
`forward` projection and its `backward` inverse, bundled with the **round-trip law**
(`scry/roundtrip.py`) so a lossy mapping is structurally caught.

This is the shape a *persistence* mapping wants — e.g. a holiday's record⇄rune mapping. The
Portfolio Manager had that mapping written three times (persistence, the form read-side, the
form write-side) and shipped a lossy-tag bug between them. One `Lens` owns the mapping once,
the app reads/writes/persists through it, and `lens.check(records)` is the regression guard:

    lens = holiday.lens()                       # forward=record→rune, backward=rune→record
    rune   = lens.forward(record)               # persist / write-side
    record = lens.backward(rune)                # read-side (rune → form)
    assert lens.check(records).ok               # backward(forward(x)) == x, for every record

Pure (no I/O) — the holiday still does the file/network; the Lens is just the mapping + law.
"""
from __future__ import annotations

from dataclasses import dataclass
from typing import Any, Callable, Iterable, Optional

from roundtrip import RoundTripReport, check_roundtrip


@dataclass
class Lens:
    forward: Callable[[Any], Any]                 # x -> y   (e.g. record -> rune)
    backward: Callable[[Any], Any]                # y -> x   (e.g. rune -> record)
    normalize: Optional[Callable[[Any], Any]] = None   # canonicalize before comparison
    label: str = "lens"

    def check(self, samples: Iterable[Any]) -> RoundTripReport:
        """The round-trip law on `forward`'s domain: `backward(forward(x)) == x` for each
        sample (e.g. every record on disk survives record→rune→record). The persistence law."""
        return check_roundtrip(self.forward, self.backward, samples,
                               normalize=self.normalize, label=self.label)

    def check_inverse(self, samples: Iterable[Any]) -> RoundTripReport:
        """The other direction: `forward(backward(y)) == y` (e.g. rune→record→rune)."""
        return check_roundtrip(self.backward, self.forward, samples,
                               normalize=self.normalize, label=f"{self.label} (inverse)")

    def inverse(self) -> "Lens":
        """The same mapping read the other way (swaps forward/backward)."""
        return Lens(self.backward, self.forward, normalize=self.normalize,
                    label=f"{self.label} (inverse)")

    # ── composition (SPEC: the pivot rule) ────────────────────────────────────
    def compose(self, other: "Lens", *, normalize: Optional[Callable[[Any], Any]] = None,
                label: Optional[str] = None) -> "Lens":
        """Chain two lenses: `self : A -> B` then `other : B -> C`, giving `A -> C`.

        This is the operator the **pivot rule** runs on — *never write a direct A->B
        adapter when A->pivot->B exists*. Direct adapters cost one per **pair**; a pivot
        costs one per **format**, and the composite inherits the round-trip law from its
        legs rather than needing its own proof.

        **The normalizer is `self.normalize or other.normalize`, not `self.normalize`.**
        `None` means *no opinion*, not *no normalization* — a composite that dropped a
        leg's canonicalization would silently weaken the guarantee its arguments came
        with, and the law would then fail on data that round-trips perfectly. (Void
        Hormiga hit exactly this composing a normalizer-less lens with a normalizing one,
        2026-08-17.)

        Two things to know about that fallback, because it is a pragmatic default and not
        a theorem. `self.normalize` canonicalizes **A**; `other.normalize` canonicalizes
        **B**. Falling back to the latter is therefore only meaningful when `self` leaves
        whatever it canonicalizes intact — which holds for the trivial or shape-preserving
        outer lens that motivates it, and is the common case. When it does not hold, pass
        `normalize=` explicitly; a wrong-domain normalizer now surfaces as a recorded
        `check` failure rather than a crash.

        And the composition law itself has a hypothesis worth stating: *lossless o
        lossless is lossless* holds exactly for **exactly-lossless** legs. When a leg is
        lossless only *up to* its normalizer, the composite is lossless only if
        `self.backward` maps `other`-equivalent values to `self`-equivalent ones. In
        practice `check` on the composite is what settles it — which is why every
        composite should be checked, not assumed.
        """
        return Lens(
            forward=lambda x: other.forward(self.forward(x)),
            backward=lambda z: self.backward(other.backward(z)),
            normalize=normalize if normalize is not None else (self.normalize or other.normalize),
            label=label or f"{self.label} -> {other.label}",
        )

    @staticmethod
    def identity(label: str = "identity") -> "Lens":
        """The unit of `compose` — passes values through untouched, and holds no opinion
        about normalization (so composing with it never weakens the other side).

        With `compose` this makes lenses a **monoid**, which is what lets `pipeline()`
        over an empty list be well-defined: a chain built from a possibly-empty list of
        lenses is then always safe to build. It is the same "identity is a cheap,
        first-class default" invariant the transformation layers already hold one level
        up (`scry` with no arguments, `Temper` with no rules)."""
        return Lens(forward=lambda x: x, backward=lambda x: x, label=label)


def pipeline(*lenses: Lens, label: Optional[str] = None) -> Lens:
    """Left-to-right composition of any number of lenses; `pipeline()` is the identity.

        record -> pivot -> wire      ==  pipeline(record_lens, wire_lens)

    Well-defined on an empty argument list precisely because `Lens.identity()` exists."""
    out = Lens.identity()
    for lens in lenses:
        out = out.compose(lens)
    if label:
        out = Lens(out.forward, out.backward, normalize=out.normalize, label=label)
    elif lenses:
        out = Lens(out.forward, out.backward, normalize=out.normalize,
                   label=" -> ".join(x.label for x in lenses))
    return out
