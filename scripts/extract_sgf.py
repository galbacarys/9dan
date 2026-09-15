#!/usr/bin/env python3
"""Pipeline (phase 1): one SGF -> compact 9x9-cropped watchface JSON.
Coordinates: SGF (row,col) 0-based. We crop to a 9x9 grid positioned so the
stone bbox is centered. Output: {"name","colors":0/1, "setup":[[x,y],...],
"line":[[idx,color,x,y],...]} — x,y within 0..8. Black=0, White=1."""
import sys, json

def parse_sgf(s):
    i=0; n=len(s)
    def skip():
        nonlocal i
        while i<n and s[i] in ' \t\r\n': i+=1
    def read_node():
        nonlocal i
        props={}
        skip()
        if i<n and s[i]==';': i+=1
        while i<n:
            skip()
            if i>=n or s[i] in '();': break
            j=i
            while j<n and s[j].isalpha(): j+=1
            key=s[i:j]; i=j
            vals=[]
            while i<n and s[i]=='[':
                i+=1; buf=[]
                while i<n:
                    c=s[i]
                    if c=='\\' and i+1<n: buf.append(s[i+1]); i+=2; continue
                    if c==']': i+=1; break
                    buf.append(c); i+=1
                vals.append(''.join(buf))
            props[key]=vals
        return props
    def seq_parse():
        nonlocal i
        skip()
        i+=1
        seq=[]
        while True:
            skip()
            if i>=n: break
            c=s[i]
            if c==')': i+=1; break
            if c=='(': i+=1; seq.append(seq_parse())
            elif c==';': i+=1; seq.append(read_node())
            else: seq.append(read_node())
        return seq
    skip()
    return seq_parse()

def coord(t):
    return (ord(t[0].upper())-65, ord(t[1].upper())-65) if len(t)==2 else None

class Node:
    def __init__(self, move, props): self.move=move; self.props=props; self.kids=[]
def build(seq):
    head=tail=None
    for item in seq:
        if isinstance(item, dict):
            m=None
            for key,color in (('B',0),('W',1)):
                for tok in item.get(key,[]):
                    c=coord(tok)
                    if c: m=(color,c); break
                if m: break
            node=Node(m,item)
            if tail: tail.kids.append(node)
            tail=node
            if head is None: head=node
        elif isinstance(item, list):
            sub=build(item)
            if sub and tail: tail.kids.append(sub)
    return head

def main():
    src=sys.argv[1]; name=sys.argv[2] if len(sys.argv)>2 else src
    s=open(src,encoding='latin-1').read()
    root=build(parse_sgf(s))
    setup=[]; line=[]
    for key,color in (('AB',0),('AW',1)):
        for tok in root.props.get(key,[]):
            c=coord(tok)
            if c: setup.append([color,c[0],c[1]])
    def mainline(node):
        if not node: return
        if node.move:
            color,(r,c)=node.move; line.append([color,r,c])
        if node.kids: mainline(node.kids[0])
    mainline(root)
    # crop to 9x9 centered on bbox of setup+line
    allpts=[[r,c] for _,r,c in setup]+[[r,c] for _,r,c in line]
    rs=[p[0] for p in allpts]; cs=[p[1] for p in allpts]
    minr,maxr=min(rs),max(rs); minc,maxc=min(cs),max(cs)
    # center a 9x9 window
    cr0 = minr - (9-(maxr-minr+1))//2
    cc0 = minc - (9-(maxc-minc+1))//2
    cr0=max(0,min(cr0, 18-8)); cc0=max(0,min(cc0,18-8))
    def t(r,c): return (r-cr0, c-cc0)
    setup9=[[color,t(r,c)[0],t(r,c)[1]] for color,r,c in setup]
    line9=[[color,t(r,c)[0],t(r,c)[1]] for color,r,c in line]
    out={'name':name,
         'setup':[[c,x,y] for c,x,y in setup9],  # full [color,x,y]
         'line':[[c,x,y] for c,x,y in line9]}
    print(json.dumps(out))

if __name__=='__main__':
    main()