# goface — Pebble C SDK notes (TOOLS.md)

Hard-won knowledge about building this watchface in C with the Pebble SDK. The
Alloy/JS lessons that apply here are kept in AGENTS.md; this file is the C
reference. Keep it current; re-read when resuming.

## Toolchain

- `pebble` CLI is a uv tool: `export PATH="$HOME/.local/bin:$PATH"`.
- SDK 4.33.1 (`pebble sdk list`); Moddable/Alloy is irrelevant here — we build
  pure C (`projectType` default, `src/c/**/*.c` in wscript).
- Repo on GitHub `galbacarys/9dan` (an experiment; PRs, not straight-to-main).

### Build & push cycle

```sh
export PATH="$HOME/.local/bin:$PATH"
pebble build                                     # emery + gabbro -> build/watchface-c.pbw
pebble install --phone <Tailscale Phone IP> build/watchface-c.pbw
pebble install --phone <Tailscale Phone IP> --logs build/watchface-c.pbw  # one-shot logs
```

- **You'll need to query tailscale to see what phones are available. Ask the user which one to install to.**
- **`--logs` is the only reliable runtime view.** Ctrl-C ends the one-shot stream.
- **`Connection refused` from a pinging phone = Developer Connection is off** in
  the Pebble app (phone-side WebSocket), NOT a network problem. Restart the app.
- Memory report prints at build: `Total footprint in RAM` / `Free RAM (heap)`.

## Memory (C is generous here)

- App heap is **128 KB**. This C build uses ~9.8 KB footprint, leaving ~121 KB
  free. The JS build (Alloy) only had ~122 KB total with ~60 KB already used by
  the XS VM — that's the OOM wall we escaped. **In C, RAM is effectively not a
  constraint.** AppMessage, settings, weather — all trivially affordable (~840 B).
- OOM signals from the JS days (`fxAbort memory full`, `Not enough memory to
  subscribe`) don't apply to C; don't chase them.

## Resources (raw data files)

- Raw `.bin` files are bundled via `package.json` → `pebble.resources.media`:
  `{ "type": "raw", "name": "EASY", "file": "easy.bin" }`.
- In C, read them with `resource_get_handle(RESOURCE_ID_EASY)`,
  `resource_size(h)`, and `resource_load_byte_range(h, 0, buf, size)`. The ID is
  the uppercase resource name. Flash-backed, loaded on demand — not in the heap.
- `resources/` holds `easy.bin`/`intermediate.bin`/`hard.bin` (regenerate with
  `scripts/build_problems.py`; see scripts/README.md).

## Controls (buttons) — the platform constraint

- **Watchfaces get NO button events, in C or JS.** Verified on-device. Up/down =
  timeline, select = launcher, back = quick-look. Do NOT rely on
  `window_single_click_subscribe` in a watchface — it never fires.
- **Only input: accelerometer.** `accel_tap_service_subscribe(handler)` with
  handler `void(AccelAxisType, int32_t)`. Firmware gesture recognition accepts a
  **shake** but not a light tap, and has no tunable threshold — accept this.
- `app_focus_service_subscribe` (in_focus) fires when the face is (re)shown → good
  hook for "new random problem each time you come back."

## System fonts (fixed sizes)

Use `fonts_get_system_font()` with a valid key, e.g.:
- `FONT_KEY_ROBOTO_CONDENSED_21` — clock.
- `FONT_KEY_GOTHIC_14` — battery %, temperature.
Other valid keys exist (LECO_*, BITHAM_*, GOTHIC_18/24/28...). Picking a
font/size that doesn't exist can crash or render wrong — stick to the known keys.

## Graphics (native C primitives — no Poco scanline hacks)

Unlike the Alloy build (which had no filled circle and a fixed-width `drawCircle`),
C has real primitives:
- `graphics_fill_circle(ctx, GPoint, radius)` — filled disc.
- `graphics_draw_circle(ctx, GPoint, radius)` with `graphics_context_set_stroke_width`
  — stroked ring at a controllable width. Note: it's a ring, not a donut — the
  interior is whatever was already drawn. To leave a stone's fill visible under a
  thin outline, fill the stone first, then stroked circle on top.
- `graphics_fill_rect(ctx, GRect, corner_radius, GCornerNone)`, `graphics_draw_line`,
  `graphics_draw_text`.
- Colors: `GColorFromRGB(r,g,b)` / `GColorFromHEX(0x...)`. Save them once at startup.

### goface drawing conventions
- Board: wood fill rect, grid lines, 5 hoshi dots at (2,2)(6,2)(2,6)(6,6)(4,4),
  stones = filled disc + 1px light outline (both colors use the same outline so
  black reads vs wood and white keeps its fill).
- Last-move marker: bright red ring just outside the stone (`radius+2`, width 2).
- Move/turn indicator (bottom-right): black or white disc showing color to play;
  a **green solid disc** when the puzzle is solved. Reset flips it back.

## AppMessage / config (Clay) / weather — the patterns we proved

- **Settings only via the phone config page** (watchface has no in-app UI). Clay
  bundles a webview that works offline. Setup:
  - `pebble package install @rebble/clay`
  - `package.json`: `"capabilities": ["configurable", "location"]` and
    `"messageKeys": ["ProblemSet","ResetSeconds","NewProblemSeconds","ClockFormat",
    "Units","TEMPERATURE","REQUEST_WEATHER"...]`.
  - `src/pkjs/config.js` (Clay JSON form) + `src/pkjs/index.js`
    (`var clay = new Clay(require('./config'));`).
  - The build auto-generates `build/include/message_keys.auto.h` →
    `MESSAGE_KEY_*`; `#include "message_keys.auto.h"` in the .c.
- **AppMessage in C:** `app_message_open(app_message_inbox_size_maximum(),
  app_message_outbox_size_maximum())` + `app_message_register_inbox_received(h)`.
  In the handler, `dict_find(iter, MESSAGE_KEY_X)`; values arrive as
  `TUPLE_CSTRING` (parse with `atoi`/`strcmp`) or `TUPLE_INT` (`.value->int32`).
- **Persist settings** with `persist_write_data`/`persist_read_data` on a plain
  struct; load at init.
- **Weather (phone-side fetch):** watch asks with `REQUEST_WEATHER`
  (`app_message_outbox_begin`/`dict_write_uint8`/`outbox_send`) on launch and every
  30 min in the tick handler. `pkjs` does `navigator.geolocation.getCurrentPosition`
  → `https://api.open-meteo.com/v1/forecast?lat=..&lon=..&current=temperature_2m`
  (+`&temperature_unit=fahrenheit` when Units says F) → sends `TEMPERATURE` back.
  Read Units from Clay's `localStorage["clay-settings"]`. Free, no API key.
- Pitfall: after adding message keys, the generated header can lag → delete
  `build/include/message_keys.auto.h` + `build/src/message_keys.auto.c` before
  rebuilding if `MESSAGE_KEY_*` comes up undeclared.

## Watch out for (anti-patterns)

- Calling `reset_board()` before a problem is assigned (order bug) leaves an empty
  board and crashes on advance. Single init path: set problem, then reset/draw.
- Random mirror/rotation (see `problems_apply_transform` in problems.c) is applied
  per problem at load — all 8 symmetries are legal on the 9×9 grid, verified.
- Rebuild + reinstall after regenerating data; don't commit stale `.bin`.
- `node_modules/` + `package-lock.json` are gitignored; run `pebble package install
  @rebble/clay` after a fresh clone.
