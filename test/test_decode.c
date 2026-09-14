// Native host test: verifies problems.c decode against the actual bundled .bin data.
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#define __FLASH 1
#include "problems.h"

#include "pebble.h"   // resource shim

const char *test_set_path = NULL;
size_t      test_file_size = 0;
FILE       *test_file = NULL;

static void close_set(void) {
  if (test_file) { fclose(test_file); test_file = NULL; }
}

static int open_set(const char *path) {
  const char *base = "/home/hermes/projects/pebble/watchface/src/embeddedjs/";
  char full[512];
  snprintf(full, sizeof(full), "%s%s", base, path);
  close_set();
  test_file = fopen(full, "rb");
  if (!test_file) { fprintf(stderr, "cannot open %s\n", full); return -1; }
  fseek(test_file, 0, SEEK_END);
  test_file_size = (size_t)ftell(test_file);
  fseek(test_file, 0, SEEK_SET);
  return 0;
}

int main(void) {
  // For each set, open the file then run problems_load + problems_get,
  // printing a deterministic dump the python side will diff.
  const char *files[] = {"easy.bin", "intermediate.bin", "hard.bin"};
  const DiffId diffs[] = {DIFF_EASY, DIFF_INTERMEDIATE, DIFF_HARD};
  int fails = 0;
  for (int d = 0; d < 3; d++) {
    if (open_set(files[d]) != 0) { fails++; continue; }
    uint16_t count = problems_load(diffs[d]);
    printf("== %s count=%u\n", files[d], count);
    for (uint16_t i = 0; i < count; i++) {
      Problem p;
      if (problems_get(i, &p) != 0) {
        printf("idx %u: GET-ERROR\n", i); fails++; continue;
      }
      printf("idx %u setup=%u line=%u\n", i, p.setup_len, p.line_len);
      // stone lines for cross-check
      fputs("S", stdout);
      for (uint16_t k = 0; k < p.setup_len; k++)
        printf(" %u:%u:%u", p.setup[k].color, p.setup[k].x, p.setup[k].y);
      fputs(" L", stdout);
      for (uint16_t k = 0; k < p.line_len; k++)
        printf(" %u:%u:%u", p.line[k].color, p.line[k].x, p.line[k].y);
      putchar('\n');
    }
  }
  close_set();
  printf("FAILS=%d\n", fails);
  return fails;
}