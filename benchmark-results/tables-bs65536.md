# Results at block_size = 65,536

Ratio is raw bytes over encoded bytes, so higher is better; every time is milliseconds,
lower better. **Bold marks the winner of a column.**

The uncompressed reference and any codec that compressed essentially nothing (ratio ≤ 1.01) are shown but never marked as winners — otherwise a codec that fell back to storing the data verbatim would win the speed columns for doing no work.

Gather uses the *clustered* trace. Uniform gather touches every chunk once the request count exceeds the chunk count, so it degenerates into a full column decode for every codec and separates nothing.

## tweet_ids — real Twitter identifiers

### 64-bit

| Codec | Ratio | Encode ms | Decode ms | Gather ms | Point ms | Sub-encodings |
|---|---|---|---|---|---|---|
| Uncompressed *(reference)* | 1.00 | 1 | 0.94 | 0.02 | 0.00 | — |
| ─────────────── | ─── | ─── | ─── | ─── | ─── | ─── |
| SubIntSplit (fixed split) | 1.09 | **1,227** | **3.51** | **2.16** | **23.72** | [0-31] UNCOMPRESSED · [32-63] BP |
| SubIntSplit (planned) | **1.54** | 2,877 | 6.09 | 3.60 | 96.04 | [0-2] BP · [3-11] BP · [12-17] BP · [18-21] RLE · [22-50] BP · [51-55] RLE · [56-63] RLE |

## snowflake — generated, dense burst

### 32-bit

| Codec | Ratio | Encode ms | Decode ms | Gather ms | Point ms | Sub-encodings |
|---|---|---|---|---|---|---|
| Uncompressed *(reference)* | 1.00 | 394 | 0.62 | 0.13 | 0.07 | — |
| ─────────────── | ─── | ─── | ─── | ─── | ─── | ─── |
| BP | 1.11 | **302** | 0.82 | 0.68 | 9.50 | — |
| PFOR | 1.12 | 310 | **0.54** | **0.34** | **3.98** | — |
| DICT | 1.00 | 892 | 1.50 | 0.26 | 0.15 | — |
| RLE | 1.11 | 742 | 4.33 | 4.59 | 77.07 | values:BP, counts:RLE, values:UNCOMPRESSED, counts:UNCOMPRESSED |
| SubIntSplit (fixed split) | 2.63 | 1,191 | 1.81 | 1.08 | 18.42 | [0-15] DICT · [16-31] RLE |
| SubIntSplit (planned) | **4.11** | 1,042 | 4.35 | 2.54 | 30.58 | [0-10] DICT · [11-18] RLE · [19-31] RLE |
| ─────────────── | ─── | ─── | ─── | ─── | ─── | ─── |
| BtrBlocks (auto, without SIS) | 1.11 | 535 | 0.98 | 0.79 | 9.93 | — |
| BtrBlocks (auto, with SIS) | **4.11** | 1,145 | 3.18 | 2.37 | 27.10 | [0-10] DICT · [11-18] RLE · [19-31] RLE |

### 64-bit

| Codec | Ratio | Encode ms | Decode ms | Gather ms | Point ms | Sub-encodings |
|---|---|---|---|---|---|---|
| Uncompressed *(reference)* | 1.00 | 2 | 2.50 | 0.04 | 0.00 | — |
| ─────────────── | ─── | ─── | ─── | ─── | ─── | ─── |
| SubIntSplit (fixed split) | 2.06 | **589** | **3.35** | **1.08** | **15.86** | [0-31] BP · [32-63] ONE_VALUE |
| SubIntSplit (planned) | **6.29** | 1,934 | 4.62 | 2.22 | 34.42 | [0-3] BP · [4-9] RLE · [10-24] DICT · [25-31] RLE · [32-63] ONE_VALUE |

## increasing — monotone, small steps

### 32-bit

| Codec | Ratio | Encode ms | Decode ms | Gather ms | Point ms | Sub-encodings |
|---|---|---|---|---|---|---|
| Uncompressed *(reference)* | 1.00 | 266 | 1.44 | 0.19 | 0.10 | — |
| ─────────────── | ─── | ─── | ─── | ─── | ─── | ─── |
| BP | 1.43 | **279** | 1.36 | 1.12 | 13.85 | — |
| PFOR | 1.44 | 301 | **0.72** | **0.47** | **5.16** | — |
| DICT | 1.00 | 752 | 1.24 | 0.20 | 0.13 | — |
| RLE | 1.43 | 704 | 7.04 | 6.42 | 97.17 | values:BP, counts:RLE, values:UNCOMPRESSED, counts:UNCOMPRESSED |
| SubIntSplit (fixed split) | 2.09 | 741 | 2.85 | 1.39 | 19.51 | [0-15] BP · [16-31] RLE |
| SubIntSplit (planned) | **4.29** | 927 | 4.15 | 2.82 | 39.30 | [0-5] BP · [6-10] RLE · [11-31] RLE |
| ─────────────── | ─── | ─── | ─── | ─── | ─── | ─── |
| BtrBlocks (auto, without SIS) | 1.43 | 328 | 1.62 | 1.28 | 16.55 | — |
| BtrBlocks (auto, with SIS) | **4.29** | 1,220 | 4.89 | 2.97 | 37.80 | [0-5] BP · [6-10] RLE · [11-31] RLE |

### 64-bit

| Codec | Ratio | Encode ms | Decode ms | Gather ms | Point ms | Sub-encodings |
|---|---|---|---|---|---|---|
| Uncompressed *(reference)* | 1.00 | 1 | 0.89 | 0.01 | 0.00 | — |
| ─────────────── | ─── | ─── | ─── | ─── | ─── | ─── |
| SubIntSplit (fixed split) | 2.87 | **257** | **1.98** | **0.92** | **13.31** | [0-31] BP · [32-63] ONE_VALUE |
| SubIntSplit (planned) | **8.58** | 1,232 | 6.20 | 3.09 | 38.00 | [0-5] BP · [6-10] RLE · [11-31] RLE · [32-63] ONE_VALUE |

## uniform — random *(control)*

### 32-bit

| Codec | Ratio | Encode ms | Decode ms | Gather ms | Point ms | Sub-encodings |
|---|---|---|---|---|---|---|
| Uncompressed *(reference)* | 1.00 | 837 | 1.70 | 0.22 | 0.12 | — |
| ─────────────── | ─── | ─── | ─── | ─── | ─── | ─── |
| BP | 1.00 | 883 | 0.61 | 0.12 | 0.07 | — |
| PFOR | 1.00 | 980 | 0.56 | 0.12 | 0.07 | — |
| DICT | 1.00 | 2,306 | 1.35 | 0.17 | 0.10 | — |
| RLE | 1.00 | 2,102 | 1.12 | 0.23 | 0.13 | — |
| SubIntSplit (fixed split) | 1.00 | 2,779 | 0.66 | 0.13 | 0.07 | — |
| SubIntSplit (planned) | 1.00 | 2,761 | 1.36 | 0.27 | 0.15 | [0-31] UNCOMPRESSED |
| ─────────────── | ─── | ─── | ─── | ─── | ─── | ─── |
| BtrBlocks (auto, without SIS) | 1.00 | 1,056 | 0.76 | 0.15 | 0.08 | — |
| BtrBlocks (auto, with SIS) | 1.00 | 1,257 | 1.28 | 0.17 | 0.10 | — |

*Nothing is marked: no codec achieved compression on this dataset, so no result here is a win.*

### 64-bit

| Codec | Ratio | Encode ms | Decode ms | Gather ms | Point ms | Sub-encodings |
|---|---|---|---|---|---|---|
| Uncompressed *(reference)* | 1.00 | 2 | 1.49 | 0.02 | 0.00 | — |
| ─────────────── | ─── | ─── | ─── | ─── | ─── | ─── |
| SubIntSplit (fixed split) | 1.00 | 2,051 | 3.26 | 1.02 | 11.58 | [0-31] UNCOMPRESSED · [32-63] UNCOMPRESSED |
| SubIntSplit (planned) | 1.00 | 4,259 | 1.45 | 0.43 | 5.06 | [0-31] UNCOMPRESSED · [32-63] UNCOMPRESSED |

*Nothing is marked: no codec achieved compression on this dataset, so no result here is a win.*
