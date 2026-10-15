"""Compare aligned event traces. Wall-clock duration is deliberately excluded."""
import argparse
import csv
import sys

FIELDS = ('frame', 'master_cycles', 'hpos', 'vpos', 'irq_latches', 'irq_missed')

def compare(a, b):
    from itertools import zip_longest
    for line, pair in enumerate(zip_longest(csv.DictReader(a), csv.DictReader(b)), 2):
        x, y = pair
        if x is None or y is None:
            return f'line {line}: different trace lengths'
        for field in FIELDS:
            if field not in x or field not in y:
                return f'line {line}: missing field {field}'
            if x[field] != y[field]:
                return f'line {line}: {field}: {x[field]} != {y[field]}'
    return None

if __name__ == '__main__':
    ap = argparse.ArgumentParser()
    ap.add_argument('reference')
    ap.add_argument('candidate')
    args = ap.parse_args()
    with open(args.reference, newline='') as a, open(args.candidate, newline='') as b:
        error = compare(a, b)
    print(error or 'Aligned frame counters match; NOT proof of cycle accuracy.')
    sys.exit(bool(error))
