# SubIntSplit

SubIntSplit decomposes each integer into contiguous bit-range sub-streams — *sections* — chosen by a
sample-driven dynamic program, and compresses each section independently through the ordinary scheme
picker.

It targets values assembled from semantic bit-fields, where different ranges of the same integer have
wildly different statistics and no single scheme fits the value as a whole. A snowflake identifier is
the canonical case:

```
 63          23           10        0
  +-----------+-----------+---------+
  | timestamp |   shard   | sequence|      41 / 13 / 10 bits
  +-----------+-----------+---------+
   slow, monotone   few distinct   cycles 0..1023
```

Bit-packing the whole value is bounded by the timestamp's magnitude. A dictionary sees a million
distinct values. RLE sees no runs, because the sequence changes every row. Split into three streams,
each field is trivial: the timestamp is long runs, the shard is a handful of values, the sequence is
a dense 10-bit counter.

Other data with the same shape: IPv4 addresses, composite keys, packed enums and flags, fixed-point
values with a coarse exponent.

## Status and scope

- **32-bit** (`IntegerSchemeType::SUB_INT_SPLIT`) is a normal registered scheme, usable anywhere in
  the Relation/Datablock/BtrReader pipeline.
- **64-bit** (`integers::SubIntSplit64`) is free-standing and driven directly, because BtrBlocks has
  no 64-bit scheme hierarchy for it to join. Its sections are still compressed by the ordinary 32-bit
  pool, so it competes against the same codecs.
- **Opt-in.** `SUB_INT_SPLIT` is deliberately absent from `defaultIntegerSchemes()`, so existing
  behaviour and benchmark numbers do not move unless it is enabled:

```cpp
BtrBlocksConfig::configure([](BtrBlocksConfig& config) {
  config.integers.schemes.enable(IntegerSchemeType::SUB_INT_SPLIT);
});
```

## Wire format

Version 1. Little-endian, packed — the destination points into the middle of a compression output
block and is never aligned.

```
struct SubIntSplitHeader {      // 4 bytes
  u8 format_version;            // 1
  u8 section_count;             // 1..max_sections
  u8 value_bits;                // 32 or 64
  u8 reorderer_id;              // 0 = none; reserved
};

struct SectionDescriptor {      // 8 bytes, section_count of them
  u8  bit_start;                // inclusive, from the least significant bit
  u8  bit_end;                  // inclusive
  u8  scheme_code;              // IntegerSchemeType, or 255 for the raw fallback
  u8  padding;
  u32 offset;                   // from the start of the payload area
};

// then: section payloads, at the recorded offsets
```

Sections are stored least-significant range first and tile `[0, value_bits)` exactly.

Two fields exist purely so the format can evolve without breaking already-written data.
`format_version` is validated on decode, so an unrecognised version is refused rather than misparsed
into a wild section count. `reorderer_id` reserves room for a sub-stream reordering transform (BWT,
MTF) to be added later.

`scheme_code` is stored per section because BtrBlocks sub-streams are not self-describing — the
parent must record what compressed each child. `255` marks the raw fallback, where the payload is the
value array verbatim; it never collides with a real code.

Decode also validates each section's bit range, since the header is on-disk format and a corrupt one
must not produce out-of-range shifts.

## Selection

Planning runs on a sample, not the full column.

1. **Sample.** Up to `sample_size` values, drawn as contiguous blocks of `sample_block_size`. Blocks
   rather than a stride because run-length statistics depend on local structure: a timestamp field
   that only changes every few thousand rows looks like noise under stride sampling and like the long
   runs it actually has under block sampling. Rows the nullmap marks absent are skipped, since null
   slots hold arbitrary bytes that would otherwise corrupt the plan.

2. **Score every candidate range.** For each bit range `[l, r]`, extract its values and estimate what
   each cost model says it would cost. The extractor extends `r` one bit at a time so the inner loop
   reuses the previous range's work. Per-sample costs are scaled to the full stream, so the split
   penalty is denominated in the same units as the savings.

3. **Dynamic program.** `dp[i][k]` is the cheapest way to cover bits `[0, i)` using exactly `k`
   sections; each additional section is charged `split_penalty`. Backtracking from the best `k` gives
   the plan. Complexity is `O(value_bits² × sample_size)` for the scoring grid, which dominates.

4. **Encode.** Each section is extracted and handed to `IntegerSchemePicker`, which chooses its scheme
   independently. The planner's prediction is recorded but not binding — keeping both lets them be
   compared, which is a useful check on the cost models.

Cost models are an interface (`ICostModel`), not a fixed set, so richer ones — speed-aware, or
weighted composites over several dimensions — can be supplied without changing the planner.

## Configuration

Under `SchemeConfig::get().integers.subintsplit`:

| Field | Default | Effect |
|---|---|---|
| `split_penalty` | 10.0 | Bits charged per extra section. Raise to split less. |
| `max_sections` | 8 | Hard cap on sections. Also a size bound — see below. |
| `max_section_bits` | 32 | Widest section. Cannot exceed 32; narrowed to 31 automatically when a sign-sensitive scheme is enabled. |
| `min_section_bits` | 1 | Narrowest section. |
| `sample_size` | 2048 | Values sampled for planning. Raising it sharpens estimates marginally and slows planning quadratically. |
| `sample_block_size` | 128 | Contiguous sample block length. 0 selects stride sampling. |

## Random access

This change also adds a random-access API to `IntegerScheme` and `BtrReader`. It is a framework
capability, available to every codec, not a SubIntSplit-private one — which is what makes the
benchmark a fair comparison rather than one scheme measured natively and the rest through ad-hoc
driver code.

**Per chunk**, on `IntegerScheme`:

```cpp
virtual void gather(INTEGER* dest, const u8* src, BitmapWrapper* nullmap, u32 tuple_count,
                    const u32* positions, u32 position_count, u32 level);
virtual INTEGER lookupAt(const u8* src, BitmapWrapper* nullmap, u32 tuple_count,
                         u32 position, u32 level);
```

The defaults decompress the chunk once into thread-local scratch and index it, so all thirteen
existing schemes get a working implementation with no per-scheme work. `gather` is
`O(tuple_count + position_count)`, never `O(position_count × tuple_count)`. `lookupAt` is left at
`O(tuple_count)` per call rather than memoizing on `src` — that is the true cost of a point lookup
against a scheme with no random access, and caching would be unsound since the caller may reuse the
buffer.

`Uncompressed`, `OneValue` and `SubIntSplit` override them.

**Per column**, on `BtrReader`:

```cpp
void gatherColumn(INTEGER* dest, const u32* positions, u32 position_count,
                  u32* chunks_touched = nullptr);
INTEGER lookupColumn(u32 position);
```

A column is stored as independently-compressed chunks, each recording its own scheme, so a scheme
instance only ever sees one chunk. `gatherColumn` buckets global positions by chunk and calls each
touched chunk's `gather` once, dispatching on that chunk's own `compression_type` exactly as
`readColumn` does. It therefore works for every registered scheme and picks up any native override
for free.

`chunks_touched` is reported because it, not the row count, is what a gather costs.

Scope: `IntegerScheme` only. `DoubleScheme` would be a mechanical addition; `StringScheme` is a
different problem (variable length, `decompressNoCopy`).

## Datasets

Four inputs, one real and three generated. Every figure below is measured over the first 1,048,576
rows — the benchmark's default — not asserted from the generator definitions.

| Dataset | Source | Width | Structure | Role |
|---|---|---|---|---|
| `tweet_ids` | Real: 30.7M Twitter IDs | 64 | Four bit-fields, timestamp near-unique per row | **The number to quote** |
| `snowflake` | Generated | 32 & 64 | Same field shape, dense burst | Upper bound: the easy case |
| `increasing` | Generated | 32 & 64 | Monotone, small steps | What bit-packing already handles |
| `uniform` | Generated | 32 & 64 | No structure | **Control:** a split must not help |

### `tweet_ids` — real Twitter identifiers

`EncodingsPlayground/Datasets/TwitterSnowflake/tweet_ids.parquet`, the same column the research
harness uses. 30,761,504 `int64` values; the benchmark reads the leading `--rows` of them.

Layout is Twitter's, and the fields behave very differently from one another:

| Field | Bits | Distinct | Range | Avg run |
|---|---|---|---|---|
| `sequence` | 0–11 (12) | 4,096 | 0–4,095 | 1.6 |
| `worker` | 12–16 (5) | 32 | 0–31 | 1.1 |
| `datacenter` | 17–21 (5) | 32 | 0–31 | 8.3 |
| `timestamp` | 22–62 (41) | 992,155 | spans ~527 days | 1.0 |

The column is **99.96% descending** — newest ID first, the usual export order — with a few large
discontinuities where sources were concatenated.

The property that actually governs compressibility is not the ordering but the **sampling density**:
these are ~1M tweets drawn across roughly 527 days, a median of ~47 minutes apart. At millisecond
resolution that means the 41-bit timestamp field takes a near-unique value on every row (992,155
distinct in 1,048,576, average run 1.0). There is simply nothing for a run- or dictionary-based scheme
to exploit in the field that occupies two thirds of the value.

### `snowflake` — generated, and deliberately the easy case

Instagram's layout: 41-bit timestamp, 13-bit shard, 10-bit sequence at 64 bits; a 21/7/4 analogue at
32 bits. The shard field carries only 16 of its 8,192 possible values (5 of 128 at 32 bits), which is
realistic — a deployment sizes the field for growth.

| Field (64-bit) | Bits | Distinct | Avg run |
|---|---|---|---|
| `sequence` | 0–9 (10) | 1,024 | 1.0 |
| `shard` | 10–22 (13) | 16 | 1.1 |
| `timestamp` | 23–63 (41) | 1,024 | **1,024.0** |

That last figure is the whole difference from the real data. The generator models a **dense burst** —
it emits 1,024 IDs per millisecond before ticking the clock — so across a million rows the timestamp
takes just 1,024 distinct values in runs of 1,024. The real column's timestamp takes 992,155 values in
runs of 1.

So `snowflake` is best read as an upper bound on what splitting can achieve, and `tweet_ids` as what
it achieves on data nobody shaped for it. The two differ by roughly 4x in ratio for exactly this
reason.

### `increasing` — monotone, with a caveat

Starts at 1,000,000 and steps by 1–8. Strictly ascending, every value distinct.

Worth being explicit about why this scores so well at 64 bits: over a million rows the values never
exceed 2^23, so **41 of the 64 bits are constant zero**. Much of the ratio there comes from isolating
that dead range rather than from anything subtle, and it flatters the 64-bit numbers. The 32-bit arm,
where only 9 bits are dead, is the more meaningful reading.

### `uniform` — the control

Full-width random values: all 64 bits live, every value distinct, no runs. There is no bit-range
structure to find, so a split cannot help. Any ratio above 1.00 here would mean the planner had
fooled itself, and the size bound is what guarantees it never lands below.

## Results

Produced by `subintsplit_bench`; see *Reproducing* below. Compression ratio is raw bytes over encoded
bytes, so higher is better.

One million rows per dataset, five repeats, median. `AUTO_BASELINE` is automatic selection with
SubIntSplit taken out of the pool — what BtrBlocks does today. `*_HALVES` is a fixed split at the
32-bit boundary (`0-15;16-31` and `0-31;32-63`), run through identical machinery so that the only
difference from `*_PLANNED` is where the boundaries fall.

See *Datasets* above for what each input is. In short: `tweet_ids` is real and is the number to
quote; the generated `snowflake` is an upper bound, because its timestamp field repeats in runs of
1,024 where the real one is near-unique per row.

### Compression ratio

`block_size = 65536`, higher is better.

| | snowflake | increasing | uniform *(control)* |
|---|---|---|---|
| **32-bit** | | | |
| `BP` | 1.11 | 1.43 | 1.00 |
| `PFOR` | 1.12 | 1.44 | 1.00 |
| `AUTO_BASELINE` | 1.11 | 1.43 | 1.00 |
| `SIS_HALVES` | 2.63 | 2.09 | 1.00 |
| **`SIS_PLANNED`** | **4.11** | **4.29** | 1.00 |
| **64-bit** | | | |
| `RAW64` | 1.00 | 1.00 | 1.00 |
| `SIS64_HALVES` | 2.06 | 2.87 | 1.00 |
| **`SIS64_PLANNED`** | **6.29** | **8.58** | 1.00 |

Three things to read out of this.

**Splitting is worth a lot on bit-field data, and choosing where to split is worth most of it.** On
64-bit snowflakes the planner reaches 6.29× where the naive halves split reaches 2.06× — so roughly
two thirds of the benefit comes from the boundaries, not from splitting per se. Snowflake fields do
not fall on a 32-bit boundary, which is exactly the case a fixed split cannot serve.

**The gain over the incumbent is large because the incumbent has nothing to work with.** Bit-packing
a snowflake is bounded by the timestamp's magnitude, so `BP` and `PFOR` manage 1.1×. That is not a
weakness of those schemes; it is what motivates splitting the value in the first place.

**Uniform data yields exactly 1.00 for every codec, including this one.** There is no bit-range
structure to find, the planner declines to split, and the size bound keeps a bad plan from ever being
worse than raw. A number above 1.00 there would mean the planner was fooling itself.

`AUTO_WITH_SIS` matches `SIS_PLANNED` on every dataset, so the picker does select the scheme when it
is available — though see limitation 5 before relying on that.

### The planner recovers the real field layout

On the real Twitter column the planner chooses:

```
0-2 ; 3-11 ; 12-17 ; 18-21 ; 22-50 ; 51-55 ; 56-63
```

Twitter's actual snowflake layout is `sequence` in bits 0–11, `worker id` in 12–16, `datacenter id`
in 17–21 and `timestamp` in 22–62. Two of the three field boundaries — **12** and **22** — are
recovered exactly, and the third lands one bit out (18 against 17). The planner is given no schema,
only 2048 sampled values.

That it rediscovers the layout from the data is the clearest evidence the cost models are measuring
something real rather than curve-fitting. It also explains the shape of the plan: the timestamp
(22–63) is subdivided further, because its high bits barely move and go to `RLE` while its low bits
do and go to `BP`; the near-constant `datacenter` range lands on `RLE`; the sequence and worker
ranges land on `BP`.

Boundary 18 rather than 17 is not really an error. `datacenter` holds only five distinct values in
1–13, so bit 21 is almost never set and the entropy boundary genuinely sits a bit above the schema
boundary.

### Speed

Snowflake, `block_size = 65536`, milliseconds. Gather is 4096 clustered positions; point access is
256 individual lookups.

| | encode | bulk decode | gather | point (256) |
|---|---|---|---|---|
| `UNCOMPRESSED` | 279 | 0.55 | 0.11 | 0.07 |
| `PFOR` | 372 | 1.07 | 0.68 | 7.95 |
| `BP` | 369 | 1.70 | 1.44 | 15.90 |
| `RLE` | 900 | 5.55 | 5.47 | 129.92 |
| `AUTO_BASELINE` | 376 | 1.37 | 1.15 | 17.91 |
| `SIS_PLANNED` (32) | 1570 | 5.38 | 4.45 | 53.42 |
| `SIS64_PLANNED` | 1383 | 7.40 | 3.46 | 51.75 |

Encode is 4× the incumbent and gets worse as blocks shrink, because planning runs per chunk: at
`block_size = 4096` the same column costs 8337 ms against 621 ms, tracking the chunk count almost
exactly. Bulk decode is roughly 4× the incumbent, which is the per-section pass structure of
limitation 1.

Point access is the honest negative result, and it is worse than parity: `SIS_PLANNED` costs 53 ms
against `PFOR`'s 8 ms, because every lookup decodes *all* sections rather than one stream.
`UNCOMPRESSED` at 0.07 ms is the only thing here doing real random access. Bit-range splitting cannot
help point workloads while the sub-schemes it delegates to have no random access of their own — see
limitation 3.

### Block size

Snowflake, 64-bit planned split.

| `block_size` | ratio | encode ms | decode ms | gather ms |
|---|---|---|---|---|
| 4096 | 6.92 | 30174 | 6.45 | 1.33 |
| 8192 | 7.05 | 8904 | 5.40 | 1.72 |
| 65536 | 6.29 | 1383 | 7.40 | 3.46 |

Smaller blocks improve decode and gather — the working set stays cache-resident, and a gather pays
for less of each chunk it barely touches — while costing dramatically more to encode, since the
planner runs once per chunk. Ratio is nearly flat, so the block size is a pure speed trade here.

The decode and gather half of that trade is a workaround for a missing capability, not a real tuning
knob: with a range-capable `decompress` the same locality would be available at any block size.

### Gather traces

Uniform gather touches every chunk at every block size tested — 16 of 16, 128 of 128, 256 of 256 —
because 4096 positions over a column of at most 256 chunks hits all of them with overwhelming
probability. In that regime every codec degenerates to a full column decode and the measurement says
nothing about any of them.

Clustered traces (1% selectivity, mean run 64) touch 118 of 256 chunks at `block_size = 4096` and 91
of 128 at 8192, so chunk locality is actually exercised. This is why the benchmark reports chunks
touched next to every gather timing, and why the numbers above use the clustered trace.

## Limitations

These are properties of BtrBlocks as it stands, not of the idea, and each points at something worth
fixing upstream.

### 1. Bulk decode makes one pass per section

`IntegerScheme::decompress` has no offset or length parameter, and BP, PFOR, DICT and RLE are not
resumable. A plan with `S` sections therefore makes `S` full passes over the chunk, each writing a
scratch buffer and reading it back to accumulate.

Nimble's implementation avoids this by decoding in 4096-row chunks, so scratch and output stay
L1/L2-resident. That optimisation is simply not expressible against this interface.

The mitigation available today is a smaller `block_size`, which is why the benchmark sweeps it rather
than fixing it — the sweep separates "SubIntSplit is slow" from "these blocks are too large for
SubIntSplit". **A range-capable `decompress` overload is the fix**, and would benefit any scheme with
sub-streams, not just this one.

### 2. Sections cannot use narrow physical types

A section is handed to `IntegerSchemePicker`, whose input is `INTEGER`, so a 12-bit section costs four
bytes per value before its sub-scheme runs. Bit-packing recovers this, but a dictionary's entries and
an RLE stream's run values do not: they stay four bytes wide regardless of section width.

This also forces the planner's cost models to charge 32 bits per value for every section, which is
less accurate than it could be. A width-parameterised sub-stream interface would recover both.

### 3. There is no point-access win

SubIntSplit's `lookupAt` could only be O(1) if every section's sub-scheme were, and none are: BP and
PFOR decompress a whole FastPFor array, DICT's codes are bit-packed, RLE has no run-offset index. Of
the default set only `UNCOMPRESSED` and `ONE_VALUE` support O(1) access — that is, exactly when
nothing is being compressed.

So a point lookup costs a chunk decode for every scheme here. For SubIntSplit it costs rather more
than that: each lookup decodes *every* section, so the cost is roughly the section count times a
single-stream scheme's. Measured, 53 ms against `PFOR`'s 8 ms for 256 lookups. Splitting makes point
access worse, not better, and no amount of tuning inside this scheme changes that.

`gather` fares better because the sections are decoded once for the whole batch rather than once per
row, but it is still a constant-factor saving on the accumulation work, not real random access.

This is reported rather than papered over because it says something concrete about what BtrBlocks
would need first: bit-range splitting cannot pay off on point workloads until the sub-schemes it
delegates to can address a row without materializing the chunk. That is the same missing capability
as limitation 1, seen from the other end.

### 4. Planning is expensive

`O(value_bits² × sample_size)`, and it runs twice per column — `SchemePicker` calls
`expectedCompressionRatio` a second time on the hot path purely for logging. SubIntSplit is by a wide
margin the slowest encoder here.

### 5. Automatic selection is unreliable for this scheme

`expectedCompressionRatio` estimates from 640 values, drawn as ten contiguous runs of 64. That is far
too few to characterise a 64-bit snowflake's timestamp cardinality, and ten contiguous runs of a
monotone field look far more compressible than the column is. Force the scheme (`EnforceScheme` or
`override_scheme`) for anything load-bearing, and treat automatic-selection hit rate as a separate
result.

## Reproducing

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j --target subintsplit_bench    # explicit target: the playground
                                                     # tools pull in the AWS SDK
./build/subintsplit_bench --rows 1048576 --block-sizes 4096,8192,65536 --repeats 5 \
                          --input-i64 build/tweet_ids.i64 --csv results.csv
```

Three datasets are generated in memory from a seed — snowflake, uniformly random, and slowly
increasing. Uniform data is a control: it has no bit-range structure, so a split cannot help, and
anything other than parity there would mean the planner is fooling itself.

### The real dataset

`--input-i64` adds a 64-bit dataset read from a flat little-endian `int64` file. BtrBlocks has no
Parquet reader and should not grow a dependency on Arrow for one benchmark input, so the conversion
happens once, out of band:

```
# Note the interpreter. The project's own .venv has pandas but not pyarrow;
# the research harness's venv has it.
../EncodingsPlayground/Benchmarks/.venv/bin/python3 tools/subintsplit/parquet_to_i64.py \
    --parquet ../EncodingsPlayground/Datasets/TwitterSnowflake/tweet_ids.parquet \
    --out build/tweet_ids.i64 --limit 4000000
```

The full column is 30,761,504 rows — 246 MB as raw `int64` — so write it under `build/` (gitignored)
or outside the repo, and use `--limit` to keep it to what the benchmark will actually read. The
converted file is **not** committed.

A file shorter than `--rows` is used as-is rather than cycled; repeating a column would manufacture
periodicity that flatters every codec measured on it. A missing or unreadable file costs that one
dataset and the sweep continues.

Tests:

```
cmake --build build -j --target tester
./build/tester --gtest_filter='SubIntSplit*:RandomAccess*'
```

The selection layer has no dependency on schemes or the wire format, so its tests
(`SubIntSplitSelector`) run standalone. `RandomAccess` is parametrised over every registered integer
scheme, which is what keeps the random-access API honest as a cross-codec baseline.

## Files

| Path | Contents |
|---|---|
| `btrblocks/scheme/integer/subintsplit/Sampler.hpp` | Block-stratified sampling |
| `btrblocks/scheme/integer/subintsplit/Metrics.hpp` | Per-range statistics |
| `btrblocks/scheme/integer/subintsplit/CostModels.hpp` | `ICostModel` and the per-scheme models |
| `btrblocks/scheme/integer/subintsplit/Selector.{hpp,cpp}` | The dynamic program |
| `btrblocks/scheme/integer/subintsplit/Plan.hpp` | Plan types, boundary strings, forced boundaries |
| `btrblocks/scheme/integer/subintsplit/SubIntSplitCore.hpp` | Wire format, encode, decode, gather |
| `btrblocks/scheme/integer/SubIntSplit.{hpp,cpp}` | The registered 32-bit scheme |
| `btrblocks/scheme/integer/SubIntSplit64.{hpp,cpp}` | The free-standing 64-bit variant |
| `tools/subintsplit/` | Benchmark driver, generators, traces |
| `tools/subintsplit/parquet_to_i64.py` | One-off Parquet → flat int64 conversion for the real dataset |
| `docs/subintsplit-porting.md` | Deviations from the Nimble original |
| `docs/subintsplit-results.csv` | Raw output behind the tables above |
