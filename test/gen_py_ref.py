import sys
ALPHABET = "!#$%&()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[]^_`abcdefghijklmnopqrst"
base = "/home/hermes/projects/pebble/watchface/src/embeddedjs/"

def toks(seg, color):
    out = []
    for ch in seg:
        p = ALPHABET.index(ch)
        out.append((color, p % 9, p // 9))
    return out

with open("/home/hermes/projects/pebble/watchface-c/test/py_out.txt", "w") as w:
    for fn, path in [("easy.bin", "easy.bin"),
                     ("intermediate.bin", "intermediate.bin"),
                     ("hard.bin", "hard.bin")]:
        data = open(base + path, "rb").read().decode("ascii")
        probs = data.split("\n")
        w.write(f"== {fn} count={len(probs)}\n")
        for i, p in enumerate(probs):
            bl, wh, ln = p.split("|")
            setup = toks(bl, 0) + toks(wh, 1)
            line = []
            for j, ch in enumerate(ln):
                p2 = ALPHABET.index(ch)
                line.append((j % 2, p2 % 9, p2 // 9))
            w.write(f"idx {i} setup={len(setup)} line={len(line)}\n")
            w.write("S " + " ".join(f"{c}:{x}:{y}" for c, x, y in setup))
            w.write(" L " + " ".join(f"{c}:{x}:{y}" for c, x, y in line) + "\n")
print("py done")