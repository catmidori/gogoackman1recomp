"""Check the fixed-field core's event cadence, separately from CPU latency."""
import argparse
import csv
from pathlib import Path

FIELD_CLOCKS = 262 * 1364

def check(rows):
    previous = None
    latency = []
    for row in rows:
        edge = int(row['vblank_edge'])
        cpu = int(row['cpu_boundary'])
        late = int(row['late_master_clocks'])
        if previous is not None and edge - previous != FIELD_CLOCKS:
            raise ValueError(f"frame {row['frame']}: event cadence {edge - previous}")
        if cpu < edge or cpu - edge != late:
            raise ValueError(f"frame {row['frame']}: inconsistent CPU/event clocks")
        latency.append(late)
        previous = edge
    if len(latency) < 2:
        raise ValueError('At least two frames required')
    return min(latency), max(latency)

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('trace', type=Path)
    args = parser.parse_args()
    with args.trace.open(newline='') as stream:
        low, high = check(csv.DictReader(stream))
    print(f'Event cadence: {FIELD_CLOCKS}; CPU latency: {low}..{high} master clocks')
