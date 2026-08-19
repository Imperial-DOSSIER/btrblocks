# Benchmark results

Generated from `subintsplit_bench` output by `tools/subintsplit/make_tables.py`. Do not edit by hand — regenerate with:

```
tools/subintsplit/run_benchmarks.sh
```

Each file compares every integer codec on every dataset at one block size. Start with [block size 65,536](tables-bs65536.md), the BtrBlocks default.

| Block size | Tables |
|---|---|
| 4,096 | [tables-bs4096.md](tables-bs4096.md) |
| 8,192 | [tables-bs8192.md](tables-bs8192.md) |
| 65,536 | [tables-bs65536.md](tables-bs65536.md) |

Block size is swept because it is the dominant lever on both decode and gather: sub-schemes have no range decode, so a plan with N sections makes N full passes per chunk, and a gather pays for whole chunks it barely touches.

Raw inputs, copied here so this directory stands alone: `subintsplit-results.csv`, `subintsplit-sections.csv`.

Narrative discussion of these numbers, with the dataset properties that explain them, is in [docs/subintsplit.md](../docs/subintsplit.md).
