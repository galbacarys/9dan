# goface — Pebble C watchface (9 × 9 go problem viewer)

The telemetry on this project lives in `TOOLS.md` (Pebble C SDK knowledge) and
this file (how this specific app is built, what must not regress, and how to
regenerate data). Re-read this file + TOOLS.md when resuming the experiment —
the knowledge is NOT auto-applied to every session.

## What goface is

A watchface that shows tsumego problems: a 9 × 9 board, setup stones, and a
solvable main line you step through. Features (all in C, all fit easily in RAM):

- 302 problems across 3 difficulty sets (`easy` 128 / `intermediate` 97 / `hard` 77),
  loaded from flash-backed raw `.bin` resources and decoded one at a time.
- Real Go capture rules on `advance()` (captured stones disappear).
- A **random mirror/rotation** is applied to every problem at load, so 302 becomes
  2,416 effective orientations (all 8 symmetries of the 9 × 9 grid; all verified legal).
- Clock (12h AM/PM or 24h), battery %, and live outside temperature (Open-Meteo via
  the phone), turn-indicator stone, red last-move ring, and a green "solved" state.
- A **phone config page** (Clay) for problem set, reset idle, new-problem idle,
  clock format, and temperature units.

## Non-negotiables (learned the hard way — DO NOT regress)

1. **Watchfaces get NO button events.** This is a platform limitation, verified
   on-device in both JS and C. Up/down = timeline, select = launcher, back =
   quick-look/backlight; the OS never delivers click callbacks to a watchface.
   `accel_tap_service_subscribe()` is the only native input. Caveat: the firmware
   gesture recognizer accepts **shakes** but not light taps, and there is no
   sensitivity knob — shake-to-advance is the intended interaction. Do not try to
   re-add button handling.

2. **Problem data lives in raw `.bin` resources, not embedded.** The generator
   (`scripts/build_problems.py`) emits `resources/{easy,intermediate,hard}.bin`
   (pure ASCII, one problem per line). `package.json` bundles them as raw
   resources; `problems.c` reads on demand via `resource_get_handle` /
   `resource_load_byte_range`. Do NOT move the problem data back into C source or
   a JS module string.

3. **Compact base-81 encoding must match EXACTLY.** The decoder in `problems.c`
   uses the same 81-char alphabet as `build_problems.py` (excludes `" ' \ |`,
   first 81 printable): `!#$%&()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[]^_`abcdefghijklmnopqrst`.
   A mismatch silently mis-decodes stones.

4. **C APIs differ from the JS/Alloy build** — see TOOLS.md. Notably: native
   `graphics_fill_circle` / `graphics_draw_circle` exist (no scanline fill
   emulation needed), but `graphics_draw_circle` strokes at the set width only.
   Fixed system fonts only (Roboto-Condensed 21, Gothic-14 for the clock/battery).

5. **Settings + weather work in C** (they were dropped in the JS build only
   because the Alloy XS heap OOM'd). In C, AppMessage adds ~840B. The config page
   is Clay in `src/pkjs/`; temp is fetched by the phone. Keep this — do not revert
   to fixed-config because that constraint no longer exists.

## Build / deploy / verify

```sh
export PATH="$HOME/.local/bin:$PATH"
pebble build                                # emery + gabbro
pebble install --phone 100.121.218.117 build/watchface-c.pbw
pebble install --phone 100.121.218.117 --logs build/watchface-c.pbw   # one-shot log stream
```

- Memory report appears at build: `Total footprint in RAM` / `Free RAM available`.
  Current: ~9.8 KB footprint, ~121 KB free heap of 128 KB. There is LOTS of
  headroom — the OOM wall that killed the JS build is gone.
- In `--logs`, `goface-c loaded: <count> set=<n>` proves init reached the end
  logger; `temp <n>` proves the weather round-trip; `settings updated` proves the
  config round-trip.
- The phone-side Pebble app must have **Developer Connection** enabled, or install
  fails with `Connection refused` even though the phone pings fine (it's the
  WebSocket, not the network). Restarting the app re-opens it.

## Regenerating problem data

```sh
python3 scripts/build_problems.py            # -> resources/*.bin (from vendor)
git diff --stat resources/*.bin              # confirm only intended changes
# rebuild + reinstall the watchface
```

Produces byte-identical output if nothing in the pipeline changed. `scripts/README.md`
has the full pipeline + independent validator (`validate_captures.py`).

## Host-side tests (no watch)

The `test/` dir compiles `problems.c` natively (with a `pebble.h` shim) and
links against the real `.bin` to verify the decoder and capture rules:

```sh
cd test
gcc -Wall -Wextra -o decode_test    test_decode.c    ../src/c/problems.c -I. -I../src/c
gcc -Wall -Wextra -o capture_test   test_capture.c   ../src/c/problems.c -I. -I../src/c
gcc -Wall -Wextra -o transform_test test_transform.c ../src/c/problems.c -I. -I../src/c
./decode_test     # all 302 decode identical to Python reference
./capture_test    # all 302 main lines simulate legally
./transform_test  # all 2,416 set*problem*sym combos stay legal
```

## Workflow rule

Change one thing, rebuild, reinstall, verify. Keep known-good committed baselines.
The experiment is on GitHub (`galbacarys/9dan`) — work on a feature branch and open
a PR rather than committing straight to `main`.