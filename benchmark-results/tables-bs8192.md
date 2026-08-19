# Results at block_size = 8,192

Ratio is raw bytes over encoded bytes, so higher is better; every time is milliseconds,
lower better. **Bold marks the winner of a column.**

The uncompressed reference and any codec that compressed essentially nothing (ratio ≤ 1.01) are shown but never marked as winners — otherwise a codec that fell back to storing the data verbatim would win the speed columns for doing no work.

Gather uses the *clustered* trace. Uniform gather touches every chunk once the request count exceeds the chunk count, so it degenerates into a full column decode for every codec and separates nothing.

## tweet_ids — real Twitter identifiers

### 64-bit

| Codec | Ratio | Encode ms | Decode ms | Gather ms | Point ms | Sub-encodings |
|---|---|---|---|---|---|---|
| Uncompressed *(reference)* | 1.00 | 1 | 1.12 | 0.02 | 0.00 | — |
| ─────────────── | ─── | ─── | ─── | ─── | ─── | ─── |
| SubIntSplit (fixed split) | 1.09 | **1,062** | **3.82** | **0.93** | **3.12** | [0-31] UNCOMPRESSED · [32-63] BP |
| SubIntSplit (planned) | **1.56** | 17,901 | 13.81 | 5.80 | 13.78 | [0-3] BP · [4-11] BP · [12-17] DICT · [18-21] RLE · [22-49] BP · [50-54] RLE · [55-63] RLE |

## snowflake — generated, dense burst

### 32-bit

| Codec | Ratio | Encode ms | Decode ms | Gather ms | Point ms | Sub-encodings |
|---|---|---|---|---|---|---|
| Uncompressed *(reference)* | 1.00 | 429 | 1.37 | 0.54 | 0.51 | — |
| ─────────────── | ─── | ─── | ─── | ─── | ─── | ─── |
| BP | 1.11 | **442** | **1.26** | **0.83** | **1.65** | — |
| PFOR | 1.12 | 479 | 2.05 | 1.60 | 3.20 | — |
| DICT | 1.00 | 1,092 | 0.55 | 0.26 | 0.27 | — |
| RLE | 1.11 | 996 | 3.32 | 2.43 | 6.31 | values:BP, counts:RLE, values:UNCOMPRESSED, counts:UNCOMPRESSED |
| SubIntSplit (fixed split) | 2.07 | 932 | 1.89 | 1.26 | 2.32 | [0-15] BP · [16-31] RLE |
| SubIntSplit (planned) | **4.11** | 5,083 | 3.62 | 1.83 | 3.33 | [0-10] BP · [11-15] RLE · [16-31] RLE |
| ─────────────── | ─── | ─── | ─── | ─── | ─── | ─── |
| BtrBlocks (auto, without SIS) | 1.11 | 545 | 2.38 | 1.80 | 4.06 | — |
| BtrBlocks (auto, with SIS) | **4.11** | 5,490 | 3.88 | 1.81 | 3.48 | [0-10] BP · [11-15] RLE · [16-31] RLE |

### 64-bit

| Codec | Ratio | Encode ms | Decode ms | Gather ms | Point ms | Sub-encodings |
|---|---|---|---|---|---|---|
| Uncompressed *(reference)* | 1.00 | 1 | 0.77 | 0.02 | 0.00 | — |
| ─────────────── | ─── | ─── | ─── | ─── | ─── | ─── |
| SubIntSplit (fixed split) | 2.06 | **508** | **3.63** | **1.03** | **2.74** | [0-31] BP · [32-63] ONE_VALUE |
| SubIntSplit (planned) | **7.05** | 8,857 | 8.11 | 2.68 | 6.22 | [0-3] BP · [4-9] RLE · [10-22] BP · [23-31] RLE · [32-63] ONE_VALUE |

## increasing — monotone, small steps

### 32-bit

| Codec | Ratio | Encode ms | Decode ms | Gather ms | Point ms | Sub-encodings |
|---|---|---|---|---|---|---|
| Uncompressed *(reference)* | 1.00 | 176 | 0.79 | 0.32 | 0.29 | — |
| ─────────────── | ─── | ─── | ─── | ─── | ─── | ─── |
| BP | 1.43 | **133** | 1.46 | 1.45 | 2.32 | — |
| PFOR | 1.44 | 210 | **1.23** | 0.85 | **1.44** | — |
| DICT | 1.00 | 360 | 0.42 | 0.22 | 0.19 | — |
| RLE | 1.43 | 404 | 4.44 | 3.33 | 9.84 | values:BP, counts:RLE, values:UNCOMPRESSED, counts:UNCOMPRESSED |
| SubIntSplit (fixed split) | 2.09 | 572 | 1.27 | **0.62** | 1.56 | [0-15] BP · [16-31] ONE_VALUE |
| SubIntSplit (planned) | **4.16** | 3,660 | 1.74 | 0.89 | 2.04 | [0-5] BP · [6-11] RLE · [12-31] RLE |
| ─────────────── | ─── | ─── | ─── | ─── | ─── | ─── |
| BtrBlocks (auto, without SIS) | 1.43 | 235 | 1.39 | 1.01 | 1.92 | — |
| BtrBlocks (auto, with SIS) | **4.16** | 4,325 | 3.24 | 1.61 | 4.80 | [0-5] BP · [6-11] RLE · [12-31] RLE |

### 64-bit

| Codec | Ratio | Encode ms | Decode ms | Gather ms | Point ms | Sub-encodings |
|---|---|---|---|---|---|---|
| Uncompressed *(reference)* | 1.00 | 3 | 2.05 | 0.05 | 0.00 | — |
| ─────────────── | ─── | ─── | ─── | ─── | ─── | ─── |
| SubIntSplit (fixed split) | 2.86 | **316** | **3.28** | 1.16 | 3.47 | [0-31] BP · [32-63] ONE_VALUE |
| SubIntSplit (planned) | **8.32** | 9,884 | 3.93 | **0.97** | **3.15** | [0-5] BP · [6-11] RLE · [12-31] RLE · [32-63] ONE_VALUE |

## uniform — random *(control)*

### 32-bit

| Codec | Ratio | Encode ms | Decode ms | Gather ms | Point ms | Sub-encodings |
|---|---|---|---|---|---|---|
| Uncompressed *(reference)* | 1.00 | 805 | 1.32 | 0.56 | 0.43 | — |
| ─────────────── | ─── | ─── | ─── | ─── | ─── | ─── |
| BP | 1.00 | 695 | 1.35 | 0.71 | 0.55 | — |
| PFOR | 1.00 | 730 | 0.82 | 0.40 | 0.34 | — |
| DICT | 1.00 | 1,911 | 1.31 | 0.61 | 0.52 | — |
| RLE | 1.00 | 1,274 | 0.48 | 0.24 | 0.21 | — |
| SubIntSplit (fixed split) | 1.00 | 2,059 | 0.91 | 0.36 | 0.36 | — |
| SubIntSplit (planned) | 1.00 | 5,236 | 0.74 | 0.40 | 0.40 | [0-31] UNCOMPRESSED |
| ─────────────── | ─── | ─── | ─── | ─── | ─── | ─── |
| BtrBlocks (auto, without SIS) | 1.00 | 626 | 2.15 | 0.63 | 0.57 | — |
| BtrBlocks (auto, with SIS) | 1.00 | 2,128 | 0.84 | 0.40 | 0.43 | — |

*Nothing is marked: no codec achieved compression on this dataset, so no result here is a win.*

### 64-bit

| Codec | Ratio | Encode ms | Decode ms | Gather ms | Point ms | Sub-encodings |
|---|---|---|---|---|---|---|
| Uncompressed *(reference)* | 1.00 | 2 | 1.92 | 0.03 | 0.00 | — |
| ─────────────── | ─── | ─── | ─── | ─── | ─── | ─── |
| SubIntSplit (fixed split) | 1.00 | 1,275 | 2.62 | 0.55 | 1.37 | [0-31] UNCOMPRESSED · [32-63] UNCOMPRESSED |
| SubIntSplit (planned) | 1.00 | 15,882 | 3.52 | 0.70 | 1.88 | [0-31] UNCOMPRESSED · [32-63] UNCOMPRESSED |

*Nothing is marked: no codec achieved compression on this dataset, so no result here is a win.*
