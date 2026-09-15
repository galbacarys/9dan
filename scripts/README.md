# goface data pipeline

Reproducible build + verification of the problem-set `.bin` resources bundled in
`resources/`. All three `.bin` files are generated from the vendored Go problems
(it's in `vendor/go-problems/`) and committed, so the pipeline is fully
self-contained.

## Regenerate the problem data

```sh
# repo defaults to vendor/go-problems; out-dir defaults to resources/
python3 scripts/build_problems.py
```

This reads every easy/intermediate/hard SGF, keeps only problems that fit a 9x9
board AND whose main line resolves under real Go capture rules, and writes:

- `resources/easy.bin` (128 problems)
- `resources/intermediate.bin` (97 problems)
- `resources/hard.bin` (77 problems)

Each `.bin` is pure ASCII, one encoded problem per line. Encoding:
`<black setup>|<white setup>|<line>`, each position = `y*9+x` as one base-81
printable char. Then rebuild + reinstall so the watch picks up the new data.

## Verify the pipeline (independent of the generator)

```sh
python3 scripts/validate_captures.py
```

Re-derives the selection/crop/encode straight from the SGF sources (does NOT parse
the generated `.bin`) and plays every main line under real capture rules. Any
problem that doesn't resolve legally is a bug in the generator. Expected: the
5 illegal problems are excluded; the 302 shipped ones all resolve.

For an end-to-end cross-check of the C decoder + capture rules against the shipped
`.bin`, there are host-side tests in `test/` (see the repo README / AGENTS.md).

## Files

- `build_problems.py` — phase 2/3: full pipeline (extract -> crop -> encode -> legal check -> `.bin`).
- `extract_sgf.py` — phase 1: single SGF -> cropped 9x9 JSON (debug/inspection helper).
- `validate_captures.py` — independent capture legality validator.
- `vendor/go-problems/` — Go Game Guru go-problems SGFs (CC BY-NC-SA 4.0), vendored in-tree.