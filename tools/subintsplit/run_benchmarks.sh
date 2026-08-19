#!/usr/bin/env bash
#
# Run the SubIntSplit benchmark sweep and render the comparison tables.
#
#   tools/subintsplit/run_benchmarks.sh            # full sweep, ~25 minutes
#   QUICK=1 tools/subintsplit/run_benchmarks.sh    # ~2 minutes, to check the pipeline
#
# Everything is overridable:
#
#   ROWS             rows per dataset                    (default 1048576)
#   BLOCK_SIZES      comma-separated                     (default 4096,8192,65536)
#   REPEATS          timing samples per measurement      (default 3)
#   BUILD_DIR        cmake build directory               (default build)
#   OUT_DIR          where the tables land               (default benchmark-results)
#   PARQUET          real dataset, Twitter IDs           (default: the research harness copy)
#   PYARROW_PYTHON   interpreter with pyarrow            (default: the harness venv)
#   TRIVIAL_RATIO    at or below this a row cannot win   (default 1.01)
#
# The real dataset is optional. Without it the sweep runs on generated data
# alone, which is the same degradation the benchmark itself applies to a missing
# --input-i64.
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$REPO_ROOT"

ROWS="${ROWS:-1048576}"
BLOCK_SIZES="${BLOCK_SIZES:-4096,8192,65536}"
REPEATS="${REPEATS:-3}"
BUILD_DIR="${BUILD_DIR:-build}"
OUT_DIR="${OUT_DIR:-benchmark-results}"
TRIVIAL_RATIO="${TRIVIAL_RATIO:-1.01}"
PARQUET="${PARQUET:-$REPO_ROOT/../EncodingsPlayground/Datasets/TwitterSnowflake/tweet_ids.parquet}"
PYARROW_PYTHON="${PYARROW_PYTHON:-$REPO_ROOT/../EncodingsPlayground/Benchmarks/.venv/bin/python3}"

# A quick pass exists to check that the whole pipeline works before committing
# 25 minutes to it. The numbers it produces are not worth quoting: one timing
# sample, one block size.
if [[ "${QUICK:-0}" != "0" ]]; then
  ROWS=262144
  BLOCK_SIZES=65536
  REPEATS=1
  echo "QUICK: ${ROWS} rows, block size ${BLOCK_SIZES}, ${REPEATS} repeat -- for checking the"
  echo "       pipeline, not for quoting."
fi

RESULTS_CSV="$BUILD_DIR/subintsplit-results.csv"
SECTIONS_CSV="$BUILD_DIR/subintsplit-sections.csv"
TWEET_I64="$BUILD_DIR/tweet_ids.i64"

echo "==> Building subintsplit_bench"
cmake -S . -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release > /dev/null
# Explicit target: the playground tools drag the AWS SDK into the whole tools
# tree, and none of it is needed here.
cmake --build "$BUILD_DIR" -j"$(nproc)" --target subintsplit_bench > /dev/null

# ---------------------------------------------------------------------------
# Real dataset. Converted once to a flat int64 file, because BtrBlocks has no
# Parquet reader and should not grow a dependency on Arrow for one input.
# ---------------------------------------------------------------------------
if [[ -f "$TWEET_I64" ]]; then
  echo "==> Reusing $TWEET_I64"
elif [[ ! -f "$PARQUET" ]]; then
  echo "==> No Parquet at $PARQUET -- running on generated data only."
  echo "    Set PARQUET=... to include the real Twitter column."
elif [[ ! -x "$PYARROW_PYTHON" ]]; then
  echo "==> No interpreter at $PYARROW_PYTHON -- running on generated data only."
  echo "    Conversion needs pyarrow; set PYARROW_PYTHON to one that has it."
else
  echo "==> Converting $PARQUET"
  # Only as many rows as the sweep will read; the full column is 246 MB.
  "$PYARROW_PYTHON" tools/subintsplit/parquet_to_i64.py \
    --parquet "$PARQUET" --out "$TWEET_I64" --limit "$ROWS"
fi

# ---------------------------------------------------------------------------
# Sweep
# ---------------------------------------------------------------------------
BENCH_ARGS=(--rows "$ROWS" --block-sizes "$BLOCK_SIZES" --repeats "$REPEATS"
            --csv "$RESULTS_CSV" --sections-csv "$SECTIONS_CSV")
if [[ -f "$TWEET_I64" ]]; then
  BENCH_ARGS+=(--input-i64 "$TWEET_I64")
fi

echo "==> Sweeping: ${ROWS} rows, block sizes ${BLOCK_SIZES}, ${REPEATS} repeats"
if [[ "${QUICK:-0}" == "0" ]]; then
  echo "    This takes roughly 25 minutes. Planning dominates encode time and runs per chunk,"
  echo "    so the small block sizes are the slow part."
fi
"$BUILD_DIR/subintsplit_bench" "${BENCH_ARGS[@]}"

# ---------------------------------------------------------------------------
# Tables. Stdlib-only, so plain python3 is enough -- no venv needed here.
# ---------------------------------------------------------------------------
echo "==> Rendering tables into $OUT_DIR"
python3 tools/subintsplit/make_tables.py \
  --results "$RESULTS_CSV" \
  --sections "$SECTIONS_CSV" \
  --out-dir "$OUT_DIR" \
  --trivial-ratio "$TRIVIAL_RATIO"

echo
echo "Done. Start at $OUT_DIR/README.md"
