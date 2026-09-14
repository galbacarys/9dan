# Python reference for Go capture sim — mirrors test_capture.c logic to cross-check.
import sys
ALPHABET = "!#$%&()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[]^_`abcdefghijklmnopqrst"
base = "/home/hermes/projects/pebble/watchface/src/embeddedjs/"
BOARD = 9

def toks(seg, color):
    return [(color, p % 9, p // 9) for p in (ALPHABET.index(c) for c in seg)]

def group(y, x, color, board):
    seen, stack = set(), [(y, x)]
    while stack:
        cy, cx = stack.pop()
        if (cy, cx) in seen: continue
        seen.add((cy, cx))
        for dy, dx in ((1,0),(-1,0),(0,1),(0,-1)):
            ny, nx = cy+dy, cx+dx
            if 0 <= ny < BOARD and 0 <= nx < BOARD and board[ny][nx] == color and (ny,nx) not in seen:
                stack.append((ny, nx))
    return seen

def has_liberty(y, x, color, board):
    return any(0 <= ny < BOARD and 0 <= nx < BOARD and board[ny][nx] == 0
               for (gy, gx) in group(y, x, color, board)
               for dy, dx in ((1,0),(-1,0),(0,1),(0,-1))
               for ny, nx in [(gy+dy, gx+dx)]
               if 0 <= ny < BOARD and 0 <= nx < BOARD and board[ny][nx] == 0)

with open("/home/hermes/projects/pebble/watchface-c/test/cap_py.txt", "w") as w:
    for fn in ("easy.bin", "intermediate.bin", "hard.bin"):
        data = open(base + fn, "rb").read().decode("ascii")
        probs = data.split("\n")
        for i, p in enumerate(probs):
            bl, wh, ln = p.split("|")
            setup = toks(bl, 0) + toks(wh, 1)
            line = [(j % 2, ALPHABET.index(c) % 9, ALPHABET.index(c) // 9) for j, c in enumerate(ln)]
            board = [[0]*BOARD for _ in range(BOARD)]
            for color, x, y in setup: board[y][x] = color + 1
            ok = True
            for color, x, y in line:
                c = color + 1
                if board[y][x] != 0: ok = False; break
                board[y][x] = c
                enemy = 3 - c
                for dy, dx in ((1,0),(-1,0),(0,1),(0,-1)):
                    ny, nx = y+dy, x+dx
                    if 0 <= ny < BOARD and 0 <= nx < BOARD and board[ny][nx] == enemy and not has_liberty(ny, nx, enemy, board):
                        for gy, gx in group(ny, nx, enemy, board): board[gy][gx] = 0
                if not has_liberty(y, x, c, board): ok = False; break
            if not ok:
                w.write(f"{fn}/{i} ILLEGAL\n"); continue
            w.write(f"{fn}/{i} " + " ".join("".join(str(board[y][x]) for x in range(BOARD)) for y in range(BOARD)) + "\n")
print("done")