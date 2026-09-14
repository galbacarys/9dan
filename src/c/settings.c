#include "settings.h"
#include <pebble.h>

#define SETTINGS_KEY 1

Settings settings_defaults(void) {
  Settings s;
  s.problem_set = 0;         // random
  s.reset_ms = 10000;        // 10s
  s.new_problem_ms = 60000;  // 60s
  s.clock_24h = false;       // 12-hour AM/PM
  return s;
}

void settings_load(Settings *out) {
  Settings def = settings_defaults();
  if (persist_exists(SETTINGS_KEY) &&
      persist_read_data(SETTINGS_KEY, out, sizeof(Settings)) == sizeof(Settings)) {
    return;
  }
  *out = def;
}

void settings_save(const Settings *s) {
  persist_write_data(SETTINGS_KEY, s, sizeof(Settings));
}