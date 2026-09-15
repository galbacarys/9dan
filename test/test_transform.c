// Host test: verify that all 8 board symmetries keep every problem legal.
// For each problem, apply each transform, then play the full main line under
// the C capture rules and require every move is legal (unoccupied, no suicide)
// and all transformed coords stay in 0..8.
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

static int sim_legal(const Problem *p, uint8_t t, int *in_range_ok) {
  Problem q = *p;
  problems_apply_transform(&q, t);
  uint8_t board[BOARD][BOARD] = {{0}};
  for (uint16_t k = 0; k < q.setup_len; k++) {
    if (q.setup[k].x > 8 || q.setup[k].y > 8) { *in_range_ok = 0; return 0; }
    board[q.setup[k].y][q.setup[k].x] = q.setup[k].color + 1;
  }
  for (uint16_t k = 0; k < q.line_len; k++) {
    Move *m = &q.line[k];
    if (m->x > 8 || m->y > 8) { *in_range_ok = 0; return 0; }
    uint8_t c = m->color + 1;
    if (board[m->y][m->x] != 0) return 0;
    board[m->y][m->x] = c;
    remove_adjacent_captures(board, m->y, m->x, c);
    if (!group_has_liberty(board, m->y, m->x, c)) return 0;
  }
  return 1;
}

int main(void) {
  const char *files[] = {"easy.bin", "intermediate.bin", "hard.bin"};
  const DiffId diffs[] = {DIFF_EASY, DIFF_INTERMEDIATE, DIFF_HARD};
  int total = 0, fails = 0, range_fails = 0;
  for (int d = 0; d < 3; d++) {
    if (open_set(files[d]) != 0) { fails++; continue; }
    uint16_t count = problems_load(diffs[d]);
    for (uint16_t i = 0; i < count; i++) {
      Problem p;
      if (problems_get(i, &p) != 0) { fails++; continue; }
      for (uint8_t t = 0; t < 8; t++) {
        total++;
        int in_range = 1;
        if (!sim_legal(&p, t, &in_range)) {
          if (!in_range) range_fails++;
          else fails++;
          printf("%s/%u t=%u: FAIL\n", files[d], i, t);
        }
      }
    }
  }
  close_set();
  printf("TOTAL=%d SYM_LEGAL_FAILS=%d RANGE_FAILS=%d\n", total, fails, range_fails);
  return fails + range_fails;
}