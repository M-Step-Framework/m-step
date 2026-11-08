#!/usr/bin/env python3
"""
adapt the raw inv-mod patterns to fit a different build.

in production mode the trace emits every single-step taken
on the secure stack. Here there are runs of latency-1 present that can differ based on the build
runs of latency-1 "settling" steps.

The script detects this amount from the trace and rewrites
each template so the exact pattern is still found. The default is a lenght of 11 based on the t6 build

Usage:
  adapt-patterns.py --trace trace.txt --out-dir OUT [--ref-period 11] \
                    --patterns f1.txt f2.txt ...
Writes each input file to OUT/<same-basename>; prints the detected period.
"""
import argparse, os
from collections import Counter

MIN_LONG = 9   # minimum of 1-runs that denote it as build dependent

def detect_period(trace_path):
    c = Counter()
    i = 0
    run = 0
    # stream the trace, counting run-lengths of value==1
    with open(trace_path) as f:
        for ln in f:
            ln = ln.strip()
            if not ln or not ln[0].isdigit():
                continue
            try:
                v = int(ln)
            except ValueError:
                continue
            if v == 1:
                run += 1
            else:
                if run:
                    c[run] += 1
                run = 0
        if run:
            c[run] += 1
    longs = {rl: n for rl, n in c.items() if rl >= MIN_LONG}
    if not longs:
        return None, c
    period = max(longs, key=longs.get)
    return period, c

def remap_nums(nums, frm, to):
    out = []
    i = 0
    n = len(nums)
    while i < n:
        if nums[i] == 1:
            j = i
            while j < n and nums[j] == 1:
                j += 1
            rl = j - i
            out += [1] * (to if rl == frm else rl)
            i = j
        else:
            out.append(nums[i]); i += 1
    return out

def adapt_file(infile, outfile, frm, to):
    lines = []
    for ln in open(infile):
        s = ln.rstrip("\n")
        if not s.strip() or s.lstrip().startswith('#'):
            lines.append(s); continue
        toks = s.split()
        name = toks[0]; rest = toks[1:]
        label = None
        if rest and not rest[0].lstrip('-').isdigit():
            label = rest[0]; rest = rest[1:]
        nums = remap_nums([int(x) for x in rest], frm, to)
        parts = [name] + ([label] if label is not None else []) + ["%02d" % x for x in nums]
        lines.append(" ".join(parts))
    with open(outfile, "w") as f:
        f.write("\n".join(lines) + "\n")

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--trace', required=True)
    ap.add_argument('--out-dir', required=True)
    ap.add_argument('--ref-period', type=int, default=11,
                    help='settling period the input templates were built at (default 11)')
    ap.add_argument('--patterns', nargs='+', required=True)
    a = ap.parse_args()

    period, _ = detect_period(a.trace)
    os.makedirs(a.out_dir, exist_ok=True)
    if period is None:
        # no long runs found (e.g. debug/filtered trace): copy unchanged
        period = a.ref_period
    print(f"[adapt-patterns] detected settling period = {period} (ref = {a.ref_period})")
    for p in a.patterns:
        adapt_file(p, os.path.join(a.out_dir, os.path.basename(p)), a.ref_period, period)
    if period != a.ref_period:
        print(f"[adapt-patterns] remapped templates {a.ref_period}->{period} into {a.out_dir}")
    else:
        print(f"[adapt-patterns] period matches reference; templates copied unchanged")

if __name__ == '__main__':
    main()
