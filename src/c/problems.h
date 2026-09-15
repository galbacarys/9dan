#ifndef GOFACE_PROBLEMS_H
#define GOFACE_PROBLEMS_H

#include <stdint.h>
#include <stdbool.h>

// Pure decoding + Go-rule logic. No graphics; renderer lives in goface.c.

#define BOARD 9
#define MAX_SETUP 48   // max setup stones across all bundled problems (measured 42)
#define MAX_LINE  24   // max main-line moves across all bundled problems (measured 19)

typedef struct {
  uint8_t color;  // 0 = black, 1 = white
  uint8_t x, y;   // 0..8
} Move;

typedef struct {
  Move setup[MAX_SETUP];
  uint16_t setup_len;
  Move line[MAX_LINE];
  uint16_t line_len;
} Problem;

typedef enum { DIFF_EASY = 0, DIFF_INTERMEDIATE, DIFF_HARD, NUM_DIFF } DiffId;

// Load the raw bytes for a difficulty set into an internal buffer.
// Returns the number of problems in the set (0 on failure).
uint16_t problems_load(DiffId diff);

// Decode problem #idx (0-based) of the currently loaded set. Returns 0 on success.
int problems_get(uint16_t idx, Problem *out);

// Board is board[y][x]: 0 empty, 1 black, 2 white.
bool group_has_liberty(const uint8_t board[BOARD][BOARD], int y, int x, uint8_t color);
// After placing stone of `color` at (y,x): remove any adjacent enemy group with no liberties.
void remove_adjacent_captures(uint8_t board[BOARD][BOARD], int y, int x, uint8_t color);

// Apply a symmetry (t in 0..7: identity, rot90 cw, rot180, rot270, mirror-x,
// mirror-y, main diagonal, anti-diagonal) to every stone's coordinates. The 9x9
// grid is invariant under all 8, orthogonal adjacency and captures are preserved,
// so a legal puzzle stays legal under every transform. Color/turn order is untouched.
void problems_apply_transform(Problem *p, uint8_t t);

#endif