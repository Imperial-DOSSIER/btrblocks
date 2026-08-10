#!/usr/bin/env python3
"""Convert one int64 Parquet column to a flat little-endian int64 binary file.

subintsplit_bench reads its real-world dataset as a raw array of int64 values,
not as Parquet. BtrBlocks has no Parquet reader, and pulling Arrow into its
build for a single benchmark input would be out of proportion to the benefit --
whereas a flat binary needs an ifstream and no dependency at all.

So this runs once, by hand, and is deliberately outside the CMake build.

Requires pyarrow, which is NOT in the project's own .venv. Use the playground's:

  EncodingsPlayground/Benchmarks/.venv/bin/python3 tools/subintsplit/parquet_to_i64.py \\
      --parquet ../EncodingsPlayground/Datasets/TwitterSnowflake/tweet_ids.parquet \\
      --out build/tweet_ids.i64 --limit 4000000

The full Twitter column is 30.7M rows -- 246 MB as raw int64 -- so write it
somewhere gitignored (build/) or outside the repo, and use --limit if the
benchmark will not read all of it.
"""

from __future__ import annotations

import argparse
import struct
import sys
from pathlib import Path

try:
    import pyarrow.parquet as pq
except ImportError:
    sys.exit(
        "pyarrow is required but not installed for this interpreter.\n"
        "The project .venv has pandas but not pyarrow; the playground one does:\n"
        "  EncodingsPlayground/Benchmarks/.venv/bin/python3"
    )


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--parquet", type=Path, required=True, help="Input Parquet file")
    parser.add_argument("--column", default="tweet_id", help="int64 column to extract")
    parser.add_argument("--out", type=Path, required=True, help="Output .i64 file")
    parser.add_argument("--limit", type=int, default=0, help="Max values to write (0 = all)")
    args = parser.parse_args()

    reader = pq.ParquetFile(args.parquet)
    field = reader.schema_arrow.field(args.column)
    if str(field.type) != "int64":
        sys.exit(f"column '{args.column}' is {field.type}, expected int64")

    args.out.parent.mkdir(parents=True, exist_ok=True)
    written = 0

    # By row group, so a 246 MB column never has to be resident all at once.
    with args.out.open("wb") as out:
        for batch in reader.iter_batches(columns=[args.column]):
            values = batch.column(0).to_pylist()
            if args.limit:
                values = values[: args.limit - written]
            if not values:
                break
            out.write(struct.pack(f"<{len(values)}q", *values))
            written += len(values)
            if args.limit and written >= args.limit:
                break

    print(f"wrote {written} int64 values ({written * 8} bytes) to {args.out}")
    if args.limit and written < args.limit:
        print(f"note: source had only {written} rows, fewer than the requested {args.limit}")


if __name__ == "__main__":
    main()
