#ifndef GOFACE_SETTINGS_H
#define GOFACE_SETTINGS_H

#include <stdint.h>
#include <stdbool.h>

// Runtime-configurable goface settings, phone -> AppMessage -> C -> persistent
// storage. Pure logic here (defaults, parse, persist); no graphics.
//
// problem_set:  0 random, 1 easy, 2 intermediate, 3 hard
// reset_ms:     inactivity before the board resets to its setup position
// new_problem_ms: inactivity before a brand-new random problem loads
// clock_24h:    0 = 12-hour AM/PM, 1 = 24-hour

typedef struct {
  int8_t  problem_set;    // 0..3
  int32_t reset_ms;       // ms
  int32_t new_problem_ms; // ms
  bool    clock_24h;
} Settings;

// Built-in defaults (used when nothing is persisted yet).
Settings settings_defaults(void);

// Load from persistent storage, falling back to defaults if never saved.
void settings_load(Settings *out);

// Persist current settings struct.
void settings_save(const Settings *s);

#endif