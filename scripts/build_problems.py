# -*- coding: utf-8 -*-
"""
Build the goface problem-set raw resources (.bin) from source SGFs.

Reads every easy/intermediate/hard problem from the gogameguru go-problems
collection, keeps only problems that fit a 9x9 board AND whose main line
resolves under real Go capture rules (legal_line), crops to that 9x9 grid,
and encodes each as a compact string:

    "<black setup positions>|<white setup positions>|<line positions>"

Position = y*9+x (0..80), 0-based on the cropped 9x9 board, encoded as a
single printable char via a base-81 alphabet. '|' separates the three groups.
Setup colors are implicit by group; line colors alternate Black/White starting
with Black (all editor problems are "Black to play").

The C watchface reads these .bin files as raw flash-backed resources and counts
problems by scanning for the newline separator (see src/c/problems.c). This
script writes one .bin per difficulty into the given output directory (default
resources/), mirroring how src/embeddedjs/ served the JS build.

Usage:
    python3 scripts/build_problems.py [gogameguru-repo-path] [out-dir] [cap]
Defaults: repo='vendor/go-problems', out-dir='resources'.
"""
import sys, os, json as _json

SEP = '|'

# 81-value printable alphabet. Excludes the JS-string-breaking chars (", ', \\)
# and our '|' separator so encoded strings are safe and unambiguous. Python and
# the C decoder (src/c/problems.c) must build the identical 81 chars.
_RESERVED = {0x22, 0x27, 0x5C, ord(SEP)}  # " ' \ and |
ALPHABET = ''.join(chr(c) for c in range(0x21, 0x7F) if c not in _RESERVED)[:81]
assert len(ALPHABET) == 81, f"alphabet has {len(ALPHABET)} chars"


# ---------- SGF parsing (shared with extract_sgf.py) ----------
def parse_sgf(s):
    i = 0; n = len(s)
    def skip():
        nonlocal i
        while i < n and s[i] in ' \t\r\n': i += 1
    def read_node():
        nonlocal i
        props = {}
        skip()
        if i < n and s[i] == ';': i += 1
        while i < n:
            skip()
            if i >= n or s[i] in '();': break
            j = i
            while j < n and s[j].isalpha(): j += 1
            key = s[i:j]; i = j
            vals = []
            while i < n and s[i] == '[':
                i += 1; buf = []
                while i < n:
                    c = s[i]
                    if c == '\\' and i+1 < n: buf.append(s[i+1]); i += 2; continue
                    if c == ']': i += 1; break
                    buf.append(c); i += 1
                vals.append(''.join(buf))
            props[key] = vals
        return props
    def seq_parse():
        nonlocal i
        skip()
        i += 1
        seq = []
        while True:
            skip()
            if i >= n: break
            c = s[i]
            if c == ')': i += 1; break
            if c == '(': i += 1; seq.append(seq_parse())
            elif c == ';': i += 1; seq.append(read_node())
            else: seq.append(read_node())
        return seq
    skip()
    return seq_parse()


def coord(t):
    return (ord(t[0].upper()) - 65, ord(t[1].upper()) - 65) if len(t) == 2 else None


class Node:
    def __init__(self, move, props):
        self.move = move; self.props = props; self.kids = []


def build(seq):
    head = tail = None
    for item in seq:
        if isinstance(item, dict):
            m = None
            for key, color in (('B', 0), ('W', 1)):
                for tok in item.get(key, []):
                    c = coord(tok)
                    if c: m = (color, c); break
                if m: break
            node = Node(m, item)
            if tail: tail.kids.append(node)
            tail = node
            if head is None: head = node
        elif isinstance(item, list):
            sub = build(item)
            if sub and tail: tail.kids.append(sub)
    return head


def extract(path):
    """Return (setup[[color,r,c]...], line[[color,r,c]...]) on the 19x19 grid."""
    s = open(path, encoding='latin-1').read()
    root = build(parse_sgf(s))
    setup = []
    for key, color in (('AB', 0), ('AW', 1)):
        for tok in root.props.get(key, []):
            c = coord(tok)
            if c: setup.append([color, c[0], c[1]])
    line = []
    def mainline(node):
        if not node: return
        if node.move:
            color, (r, c) = node.move; line.append([color, r, c])
        if node.kids: mainline(node.kids[0])
    mainline(root)
    return setup, line


# ---------- encoding ----------
def crop9(setup, line):
    """Map [color,r,c] lists onto a cropped 9x9 grid centered on the stone bbox.
    Returns setup9,line9 (still lists) or None if the bbox doesn't fit 9x9."""
    allpts = [[r, c] for _, r, c in setup] + [[r, c] for _, r, c in line]
    if not allpts:
        return None
    rs = [p[0] for p in allpts]; cs = [p[1] for p in allpts]
    minr, maxr = min(rs), max(rs)
    minc, maxc = min(cs), max(cs)
    if maxr - minr + 1 > 9 or maxc - minc + 1 > 9:
        return None
    # Center the 9x9 crop on the stone bbox, clamped so translated coords stay
    # in 0..8. Centering preserves surrounding margin for mid-board groups so
    # edge placement (which changes Go positions) isn't introduced where none exists.
    want_r = 9 - (maxr - minr + 1)
    want_c = 9 - (maxc - minc + 1)
    cr0 = minr - want_r // 2
    cc0 = minc - want_c // 2
    cr0 = min(cr0, maxr - 8)
    cc0 = min(cc0, maxc - 8)
    cr0 = max(0, cr0); cc0 = max(0, cc0)
    t = lambda r, c: (r - cr0, c - cc0)
    setup9 = [[color, *t(r, c)] for color, r, c in setup]
    line9 = [[color, *t(r, c)] for color, r, c in line]
    return setup9, line9


def encode(setup9, line9):
    black = ''.join(ALPHABET[y * 9 + x] for _, y, x in setup9 if _ == 0)
    white = ''.join(ALPHABET[y * 9 + x] for _, y, x in setup9 if _ == 1)
    line = ''.join(ALPHABET[y * 9 + x] for _, y, x in line9)
    return black + SEP + white + SEP + line


def legal_line(black_pos, white_pos, line_pos):
    """Play the main line under real Go capture rules on a 9x9 board.
    Returns True if every move is legal (never plays on an occupied point, no
    suicide). Problems with illegal first-variation moves are excluded from the
    bundle so the watch never renders a broken position."""
    N = 9
    board = [[0] * N for _ in range(N)]
    for y, x in black_pos: board[y][x] = 1
    for y, x in white_pos: board[y][x] = 2

    def group(y, x, color):
        seen, stack = set(), [(y, x)]
        while stack:
            cy, cx = stack.pop()
            if (cy, cx) in seen: continue
            seen.add((cy, cx))
            for dy, dx in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                ny, nx = cy + dy, cx + dx
                if 0 <= ny < N and 0 <= nx < N and board[ny][nx] == color and (ny, nx) not in seen:
                    stack.append((ny, nx))
        return seen

    def libs(grp):
        l = set()
        for gy, gx in grp:
            for dy, dx in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                ny, nx = gy + dy, gx + dx
                if 0 <= ny < N and 0 <= nx < N and board[ny][nx] == 0:
                    l.add((ny, nx))
        return l

    for i, (y, x) in enumerate(line_pos):
        color = 1 if i % 2 == 0 else 2
        if board[y][x] != 0:
            return False
        board[y][x] = color
        for dy, dx in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            ny, nx = y + dy, x + dx
            if 0 <= ny < N and 0 <= nx < N and board[ny][nx] == 3 - color:
                grp = group(ny, nx, 3 - color)
                if not libs(grp):
                    for gy, gx in grp: board[gy][gx] = 0
        if not libs(group(y, x, color)):
            return False
    return True


# ---------- main ----------
def main():
    if len(sys.argv) > 1 and sys.argv[1] in ("-h", "--help"):
        print("usage: build_problems.py [repo] [out-dir] [cap]"); sys.exit(0)
    repo = sys.argv[1] if len(sys.argv) > 1 else 'vendor/go-problems'
    out_dir = sys.argv[2] if len(sys.argv) > 2 else 'resources'
    cap = int(sys.argv[3]) if len(sys.argv) > 3 else None
    os.makedirs(out_dir, exist_ok=True)

    sets = {}
    for diff in ('easy', 'intermediate', 'hard'):
        d = os.path.join(repo, 'weekly-go-problems', diff)
        if not os.path.isdir(d):
            print(f"warning: no dir {d}"); continue
        encoded = []
        for f in sorted(os.listdir(d)):
            if not f.endswith('.sgf'): continue
            setup, line = extract(os.path.join(d, f))
            cropped = crop9(setup, line)
            if cropped is None:
                continue  # doesn't fit a 9x9 board; skip
            setup9, line9 = cropped
            black_pos = [(y, x) for _, y, x in setup9 if _ == 0]
            white_pos = [(y, x) for _, y, x in setup9 if _ == 1]
            line_pos = [(y, x) for _, y, x in line9]
            if not legal_line(black_pos, white_pos, line_pos):
                print(f"  skip {diff}/{f}: illegal main line under capture rules")
                continue
            encoded.append(encode(setup9, line9))
            if cap is not None and len(encoded) >= cap:
                break
        sets[diff] = encoded
        print(f"{diff}: {len(encoded)} problems encoded")

    # Emit one raw resource per set: problems joined by '\n' (pure ASCII so the
    # watch reads raw bytes directly and counts problems by newline).
    counts = {}
    for diff in ('easy', 'intermediate', 'hard'):
        items = sets.get(diff, [])
        counts[diff] = len(items)
        path = os.path.join(out_dir, f'{diff}.bin')
        with open(path, 'wb') as bf:
            bf.write('\n'.join(items).encode('ascii'))
        print(f"{diff}.bin: {len(items)} problems -> {path}")

    total = sum(counts.values())
    print(f"TOTAL: {total} problems")


if __name__ == '__main__':
    main()