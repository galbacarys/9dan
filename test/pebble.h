// Minimal pebble.h shim so problems.c can be compiled and tested natively on the host.
// Only the resource API + types that problems.c actually uses.
#ifndef PEBBLE_SHIM_H
#define PEBBLE_SHIM_H
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct ResHandle_ *ResHandle;

#define RESOURCE_ID_EASY         10
#define RESOURCE_ID_INTERMEDIATE 11
#define RESOURCE_ID_HARD         12

// opaque simulation of the bundled .bin bytes (set by test harness before load)
extern const char *test_set_path;
extern size_t      test_file_size;
extern FILE       *test_file;

static inline size_t resource_size(ResHandle h) {
  (void)h;
  return test_file_size;
}
static inline size_t resource_load_byte_range(ResHandle h, uint32_t off,
                                              uint8_t *buf, size_t n) {
  (void)h;
  if (!test_file) {
    // no file, return the requested count to exercise counting (shouldn't happen
    // for the real test which always opens a file)
    return n;
  }
  fseek(test_file, (long)off, SEEK_SET);
  return fread(buf, 1, n, test_file);
}
static inline ResHandle resource_get_handle(int id) {
  (void)id;
  return (ResHandle)(intptr_t)7;
}
#endif