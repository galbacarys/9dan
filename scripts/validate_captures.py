#!/usr/bin/env python3
"""Validate that every OUTER-SHELL-SELECTED problem's main line resolves under
real Go capture rules (no illegal move, no suicide). Reads straight from the
gogameguru repo (same selection/crop/encode as build_problems) so there's no
dependency on parsing the generated JS."""
import sys, os
sys.path.insert(0, 'scripts')
import build_problems as bp

def dec(ch):
    return bp.ALPHABET.index(ch)

def resolve_captures(black0, white0, line):
    N = 9
    board = [[0]*N for _ in range(N)]
    for (y, x) in black0:
        if not (0 <= y < N and 0 <= x < N): return None, "setup out of range"
        board[y][x] = 1
    for (y, x) in white0:
        if not (0 <= y < N and 0 <= x < N): return None, "setup out of range"
        board[y][x] = 2

    def group(y, x, color):
        seen, stack = set(), [(y, x)]
        while stack:
            cy, cx = stack.pop()
            if (cy, cx) in seen: continue
            seen.add((cy, cx))
            for dy, dx in ((1,0),(-1,0),(0,1),(0,-1)):
                ny, nx = cy+dy, cx+dx
                if 0 <= ny < N and 0 <= nx < N and board[ny][nx] == color and (ny, nx) not in seen:
                    stack.append((ny, nx))
        return seen

    def group_liberties(grp):
        libs = set()
        for (gy, gx) in grp:
            for dy, dx in ((1,0),(-1,0),(0,1),(0,-1)):
                ny, nx = gy+dy, gx+dx
                if 0 <= ny < N and 0 <= nx < N and board[ny][nx] == 0:
                    libs.add((ny, nx))
        return libs

    seq = [1 if i % 2 == 0 else 2 for i in range(len(line))]
    for (y, x), color in zip(line, seq):
        if not (0 <= y < N and 0 <= x < N): return None, "line out of range"
        if board[y][x] != 0:
            return None, f"occupied ({y},{x}) played"
        board[y][x] = color
        for dy, dx in ((1,0),(-1,0),(0,1),(0,-1)):
            ny, nx = y+dy, x+dx
            if 0 <= ny < N and 0 <= nx < N and board[ny][nx] == 3-color:
                grp = group(ny, nx, 3-color)
                if not group_liberties(grp):
                    for (gy, gx) in grp: board[gy][gx] = 0
        if not group_liberties(group(y, x, color)):
            return None, f"suicide at ({y},{x})"
    return board, None

def main():
    repo = sys.argv[1]
    ok = bad = 0
    captures = 0
    for diff in ('easy', 'intermediate', 'hard'):
        d = os.path.join(repo, 'weekly-go-problems', diff)
        items = []
        for f in sorted(os.listdir(d)):
            if not f.endswith('.sgf'): continue
            setup, line = bp.extract(os.path.join(d, f))
            cr = bp.crop9(setup, line)
            if cr is None: continue
            items.append(bp.encode(*cr))
        badd = 0
        for enc in items:
            b_s, w_s, l_s = enc.split(bp.SEP)
            def stones(s): return [(dec(c)//9, dec(c)%9) for c in s]
            black, white, linep = stones(b_s), stones(w_s), stones(l_s)
            board, err = resolve_captures(black, white, linep)
            if board is None:
                badd += 1; bad += 1
                print(f"  {diff} {enc!r}: {err}")
            else:
                ok += 1
            if len(linep) != len(set(linep)):
                captures += 1
        print(f"{diff}: {len(items)} problems, {badd} illegal")
    print(f"\nRESULT: {ok} legally resolve, {bad} illegal; {captures} have repeated move-points (captures)")

if __name__ == '__main__':
    main()