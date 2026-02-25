#!/usr/bin/env python3
"""analyze_consumer_test.py

Simple analyzer for consumer test CSV files.
Usage: analyze_consumer_test.py <input_csv>

Reads CSV rows where the second column is a nanosecond timestamp.
Prints count, mean/min/max/stddev of inter-sample deltas (in ms) and
how many deltas fall within 5±0.1 ms and 5±0.5 ms.
"""

import sys
import csv
import statistics


def analyze(path, show_first=10):
    ts = []
    with open(path, newline='') as f:
        for row in csv.reader(f):
            if not row:
                continue
            # Skip comments and header-like lines
            if row[0].startswith('#'):
                continue
            # Expect timestamp at index 1
            if len(row) < 2:
                continue
            try:
                t = int(row[1])
            except Exception:
                # Try to strip possible whitespace or header
                try:
                    t = int(row[1].strip())
                except Exception:
                    continue
            ts.append(t)

    if len(ts) < 2:
        print(f"Not enough timestamps in {path} (found {len(ts)})")
        return 1

    diffs = [ts[i+1] - ts[i] for i in range(len(ts)-1)]
    ms = [d / 1e6 for d in diffs]

    mean_ms = statistics.mean(ms)
    stdev_ms = statistics.pstdev(ms) if len(ms) > 1 else 0.0
    min_ms = min(ms)
    max_ms = max(ms)
    count = len(ms)

    within_01 = sum(1 for x in ms if abs(x - 5.0) <= 0.1)
    within_05 = sum(1 for x in ms if abs(x - 5.0) <= 0.5)

    print(f"File: {path}")
    print(f"Deltas count: {count}")
    print(f"Mean delta: {mean_ms:.6f} ms")
    print(f"Stddev (population): {stdev_ms:.6f} ms")
    print(f"Min delta: {min_ms:.6f} ms, Max delta: {max_ms:.6f} ms")
    print()
    print(f"Within 5±0.1 ms: {within_01}/{count} ({within_01/count*100:.2f}%)")
    print(f"Within 5±0.5 ms: {within_05}/{count} ({within_05/count*100:.2f}%)")
    print()
    print(f"Sample of first {min(show_first, count)} deltas (ms):")
    for i in range(min(show_first, count)):
        print(f"{i+1}: {ms[i]:.6f} ms")

    return 0


if __name__ == '__main__':
    if len(sys.argv) < 2:
        print("Usage: analyze_consumer_test.py <input_csv>")
        sys.exit(1)
    sys.exit(analyze(sys.argv[1]))
