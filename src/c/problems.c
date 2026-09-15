#include "problems.h"
#include <pebble.h>

// Base-81 printable alphabet. Must EXACTLY match scripts/build_problems.py:
// chars 0x21..0x7E excluding " (0x22), ' (0x27), \ (0x5C), | (0x7C), then take
// the first 81. A mismatch silently mis-decodes stones.
static const char ALPHABET[] =
  "!#$%&()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[]^_`abcdefghijklmnopqrst";

// --- internal set buffer -----------------------------------------------------
static uint8_t s_bytes[4096];
static uint32_t s_size = 0;
static uint16_t s_count = 0;

static int alphabet_index(uint8_t c) {
  for (uint16_t i = 0; i < 81; i++) {
    if ((uint8_t)ALPHABET[i] == c) return (int)i;
  }
  return -1;
}

uint16_t problems_load(DiffId diff) {
  uint8_t id;
  switch (diff) {
    case DIFF_EASY:         id = RESOURCE_ID_EASY; break;
    case DIFF_INTERMEDIATE: id = RESOURCE_ID_INTERMEDIATE; break;
    case DIFF_HARD:         id = RESOURCE_ID_HARD; break;
    default: return 0;
  }
  ResHandle h = resource_get_handle(id);
  uint32_t size = resource_size(h);
  if (size == 0 || size > sizeof(s_bytes)) return 0;
  if (resource_load_byte_range(h, 0, s_bytes, size) != size) return 0;
  s_size = size;
  s_count = 0;
  for (uint32_t i = 0; i < s_size; i++) {
    if (s_bytes[i] == '\n') s_count++;
  }
  s_count++; // N newlines => N+1 problems
  return s_count;
}

int problems_get(uint16_t idx, Problem *out) {
  if (!s_size || idx >= s_count) return -1;
  // locate [start,end) of problem #idx
  uint32_t start = 0, end = s_size, seen = 0;
  for (uint32_t i = 0; i < s_size; i++) {
    if (seen == idx) { start = i; break; }
    if (s_bytes[i] == '\n') { seen++; start = i + 1; }
  }
  for (uint32_t i = start; i < s_size; i++) {
    if (s_bytes[i] == '\n') { end = i; break; }
  }

  out->setup_len = 0;
  out->line_len = 0;

  // "<black set>|<white set>|<line>", split on '|'
  const uint8_t *b = s_bytes;
  int group = 0;      // 0 black setup, 1 white setup, 2 line
  int line_i = 0;
  for (uint32_t i = start; i < end; i++) {
    uint8_t c = b[i];
    if (c == '|') { group++; continue; }
    int p = alphabet_index(c);
    if (p < 0) return -1;
    uint8_t x = (uint8_t)(p % 9), y = (uint8_t)(p / 9);

    if (group == 0 || group == 1) {
      if (out->setup_len >= MAX_SETUP) return -1;
      Move *m = &out->setup[out->setup_len++];
      m->color = (group == 0) ? 0 : 1;
      m->x = x; m->y = y;
    } else {
      if (out->line_len >= MAX_LINE) return -1;
      Move *m = &out->line[out->line_len++];
      m->color = (uint8_t)(line_i % 2); // B,W,B,W...
      m->x = x; m->y = y;
      line_i++;
    }
  }
  return 0;
}

// --- Go capture rules --------------------------------------------------------
static bool group_has_liberty_r(const uint8_t board[BOARD][BOARD],
                                bool seen[BOARD][BOARD], int y, int x, uint8_t color) {
  if (seen[y][x]) return 0;
  seen[y][x] = true;
  const int dy[4] = {-1, 1, 0, 0};
  const int dx[4] = {0, 0, -1, 1};
  for (int d = 0; d < 4; d++) {
    int ny = y + dy[d], nx = x + dx[d];
    if (ny < 0 || ny >= BOARD || nx < 0 || nx >= BOARD) continue;
    if (board[ny][nx] == 0) return true;
    if (board[ny][nx] == color &&
        group_has_liberty_r(board, seen, ny, nx, color)) return true;
  }
  return false;
}

bool group_has_liberty(const uint8_t board[BOARD][BOARD], int y, int x, uint8_t color) {
  bool seen[BOARD][BOARD] = {false};
  return group_has_liberty_r(board, seen, y, x, color);
}

static void flood_remove(uint8_t board[BOARD][BOARD], bool done[BOARD][BOARD],
                         int y, int x, uint8_t color) {
  if (board[y][x] != color || done[y][x]) return;
  done[y][x] = true;
  board[y][x] = 0;
  const int dy[4] = {-1, 1, 0, 0};
  const int dx[4] = {0, 0, -1, 1};
  for (int d = 0; d < 4; d++) {
    int ny = y + dy[d], nx = x + dx[d];
    if (ny < 0 || ny >= BOARD || nx < 0 || nx >= BOARD) continue;
    flood_remove(board, done, ny, nx, color);
  }
}

void remove_adjacent_captures(uint8_t board[BOARD][BOARD], int y, int x, uint8_t color) {
  uint8_t enemy = (uint8_t)(3 - color);
  const int dy[4] = {-1, 1, 0, 0};
  const int dx[4] = {0, 0, -1, 1};
  for (int d = 0; d < 4; d++) {
    int ny = y + dy[d], nx = x + dx[d];
    if (ny < 0 || ny >= BOARD || nx < 0 || nx >= BOARD) continue;
    if (board[ny][nx] == enemy && !group_has_liberty(board, ny, nx, enemy)) {
      bool done[BOARD][BOARD] = {false};
      flood_remove(board, done, ny, nx, enemy);
    }
  }
}

// Map a board coordinate (0..8) through one of the 8 dihedral symmetries of the
// square grid. N = BOARD-1 = 8 (max valid coordinate).
static void sym_map(uint8_t t, uint8_t *x, uint8_t *y) {
  const uint8_t N = BOARD - 1;
  uint8_t X = *x, Y = *y;
  switch (t) {
    case 0: break;                          // identity
    case 1: *x = N - Y; *y = X; break;       // rot90 clockwise
    case 2: *x = N - X; *y = N - Y; break;   // rot180
    case 3: *x = Y; *y = N - X; break;       // rot270 clockwise
    case 4: *x = N - X; *y = Y; break;       // reflect across vertical midline (mirror x)
    case 5: *x = X; *y = N - Y; break;       // reflect across horizontal midline (mirror y)
    case 6: *x = Y; *y = X; break;           // main diagonal (transpose)
    case 7: *x = N - Y; *y = N - X; break;   // anti-diagonal
    default: break;
  }
}

void problems_apply_transform(Problem *p, uint8_t t) {
  for (uint16_t i = 0; i < p->setup_len; i++) sym_map(t, &p->setup[i].x, &p->setup[i].y);
  for (uint16_t i = 0; i < p->line_len; i++) sym_map(t, &p->line[i].x, &p->line[i].y);
}