// Native host test: simulate advancing through every problem's main line under
// the C capture rules and dump the final board, so a Python sim can diff it.
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include "problems.h"
#include "pebble.h"

const char *test_set_path = NULL;
size_t      test_file_size = 0;
FILE       *test_file = NULL;

static void close_set(void) { if (test_file) { fclose(test_file); test_file = NULL; } }
static int open_set(const char *path) {
  const char *base = "/home/hermes/projects/pebble/watchface/src/embeddedjs/";
  char full[512];
  snprintf(full, sizeof(full), "%s%s", base, path);
  close_set();
  test_file = fopen(full, "rb");
  if (!test_file) return -1;
  fseek(test_file, 0, SEEK_END);
  test_file_size = (size_t)ftell(test_file);
  fseek(test_file, 0, SEEK_SET);
  return 0;
}

int main(void) {
  const char *files[] = {"easy.bin", "intermediate.bin", "hard.bin"};
  const DiffId diffs[] = {DIFF_EASY, DIFF_INTERMEDIATE, DIFF_HARD};
  int fails = 0;
  for (int d = 0; d < 3; d++) {
    if (open_set(files[d]) != 0) { fails++; continue; }
    uint16_t count = problems_load(diffs[d]);
    for (uint16_t i = 0; i < count; i++) {
      Problem p;
      if (problems_get(i, &p) != 0) { fails++; continue; }
      uint8_t board[BOARD][BOARD] = {{0}};
      for (uint16_t k = 0; k < p.setup_len; k++) {
        Move *m = &p.setup[k];
        board[m->y][m->x] = m->color + 1;
      }
      // step through the line; if any move is illegal (occupied / suicide), fail
      int ok = 1;
      for (uint16_t k = 0; k < p.line_len; k++) {
        Move *m = &p.line[k];
        uint8_t color = m->color + 1;
        if (board[m->y][m->x] != 0) { ok = 0; break; }
        board[m->y][m->x] = color;
        remove_adjacent_captures(board, m->y, m->x, color);
        if (!group_has_liberty(board, m->y, m->x, color)) { ok = 0; break; }
      }
      if (!ok) {
        printf("%s/%u ILLEGAL\n", files[d], i);
        fails++;
        continue;
      }
      // final board line, deterministic
      printf("%s/%u", files[d], i);
      for (int y = 0; y < BOARD; y++) {
        fputc(' ', stdout);
        for (int x = 0; x < BOARD; x++) fputc("012"[board[y][x] & 3], stdout);
      }
      putchar('\n');
    }
  }
  close_set();
  printf("ILLEGAL_COUNT=%d\n", fails);
  return fails;
}