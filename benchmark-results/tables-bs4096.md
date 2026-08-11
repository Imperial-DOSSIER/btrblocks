# Results at block_size = 4,096

Ratio is raw bytes over encoded bytes, so higher is better; every time is milliseconds,
lower better. **Bold marks the winner of a column.**

The uncompressed reference and any codec that compressed essentially nothing (ratio ≤ 1.01) are shown but never marked as winners — otherwise a codec that fell back to storing the data verbatim would win the speed columns for doing no work.

Gather uses the *clustered* trace. Uniform gather touches every chunk once the request count exceeds the chunk count, so it degenerates into a full column decode for every codec and separates nothing.

## tweet_ids — real Twitter identifiers

### 64-bit

| Codec | Ratio | Encode ms | Decode ms | Gather ms | Point ms | Sub-encodings |
|---|---|---|---|---|---|---|
| Uncompressed *(reference)* | 1.00 | 2 | 2.15 | 0.03 | 0.00 | — |
| ─────────────── | ─── | ─── | ─── | ─── | ─── | ─── |
| SubIntSplit (fixed split) | 1.09 | **1,125** | **2.44** | **1.02** | **1.39** | [0-31] UNCOMPRESSED · [32-63] BP |
| SubIntSplit (planned) | **1.57** | 31,208 | 13.86 | 4.26 | 8.35 | [0-3] BP · [4-11] BP · [12-16] DICT · [17-21] DICT · [22-48] BP · [49-53] RLE · [54-63] RLE |

## snowflake — generated, dense burst

### 32-bit

| Codec | Ratio | Encode ms | Decode ms | Gather ms | Point ms | Sub-encodings |
|---|---|---|---|---|---|---|
| Uncompressed *(reference)* | 1.00 | 274 | 0.40 | 0.30 | 0.39 | — |
| ─────────────── | ─── | ─── | ─── | ─── | ─── | ─── |
| BP | 1.11 | 373 | 1.27 | 0.69 | 1.11 | — |
| PFOR | 1.12 | **301** | 1.36 | 0.71 | 1.30 | — |
| DICT | 1.00 | 665 | 0.74 | 0.44 | 0.46 | — |
| RLE | 1.11 | 715 | 4.83 | 2.16 | 5.01 | values:BP, counts:RLE, values:UNCOMPRESSED, counts:UNCOMPRESSED |
| SubIntSplit (fixed split) | 2.06 | 776 | **1.02** | **0.59** | **1.06** | [0-15] BP · [16-31] RLE |
| SubIntSplit (planned) | **4.03** | 5,754 | 1.46 | 0.87 | 1.32 | [0-10] BP · [11-14] RLE · [15-31] RLE |
| ─────────────── | ─── | ─── | ─── | ─── | ─── | ─── |
| BtrBlocks (auto, without SIS) | 1.11 | 584 | 1.29 | 0.74 | 1.56 | — |
| BtrBlocks (auto, with SIS) | **4.03** | 7,541 | 1.91 | 1.01 | 1.80 | [0-10] BP · [11-14] RLE · [15-31] RLE |

### 64-bit

| Codec | Ratio | Encode ms | Decode ms | Gather ms | Point ms | Sub-encodings |
|---|---|---|---|---|---|---|
| Uncompressed *(reference)* | 1.00 | 1 | 1.17 | 0.03 | 0.00 | — |
| ─────────────── | ─── | ─── | ─── | ─── | ─── | ─── |
| SubIntSplit (fixed split) | 2.05 | **390** | **2.60** | **0.38** | **0.60** | [0-31] BP · [32-63] ONE_VALUE |
| SubIntSplit (planned) | **6.92** | 12,743 | 3.38 | 0.73 | 1.45 | [0-3] BP · [4-9] RLE · [10-22] BP · [23-31] RLE · [32-63] ONE_VALUE |

## increasing — monotone, small steps

### 32-bit

| Codec | Ratio | Encode ms | Decode ms | Gather ms | Point ms | Sub-encodings |
|---|---|---|---|---|---|---|
| Uncompressed *(reference)* | 1.00 | 126 | 0.47 | 0.34 | 0.37 | — |
| ─────────────── | ─── | ─── | ─── | ─── | ─── | ─── |
| BP | 1.43 | **137** | **1.05** | 0.62 | 1.09 | — |
| PFOR | 1.44 | 155 | 1.11 | 0.77 | 1.18 | — |
| DICT | 1.00 | 345 | 0.48 | 0.35 | 0.36 | — |
| RLE | 1.42 | 331 | 3.20 | 1.60 | 3.46 | values:BP, counts:RLE, values:UNCOMPRESSED, counts:UNCOMPRESSED |
| SubIntSplit (fixed split) | 2.08 | 367 | 1.30 | **0.56** | **0.89** | [0-15] BP · [16-31] ONE_VALUE |
| SubIntSplit (planned) | **4.08** | 5,062 | 1.94 | 0.94 | 1.75 | [0-5] BP · [6-10] RLE · [11-31] RLE |
| ─────────────── | ─── | ─── | ─── | ─── | ─── | ─── |
| BtrBlocks (auto, without SIS) | 1.43 | 181 | 1.30 | 0.89 | 1.44 | — |
| BtrBlocks (auto, with SIS) | **4.08** | 6,353 | 1.94 | 0.77 | 1.26 | [0-5] BP · [6-10] RLE · [11-31] RLE |

### 64-bit

| Codec | Ratio | Encode ms | Decode ms | Gather ms | Point ms | Sub-encodings |
|---|---|---|---|---|---|---|
| Uncompressed *(reference)* | 1.00 | 1 | 1.02 | 0.02 | 0.00 | — |
| ─────────────── | ─── | ─── | ─── | ─── | ─── | ─── |
| SubIntSplit (fixed split) | 2.86 | **189** | **2.80** | **0.56** | **1.17** | [0-31] BP · [32-63] ONE_VALUE |
| SubIntSplit (planned) | **8.17** | 17,532 | 4.87 | 1.01 | 2.26 | [0-5] BP · [6-10] RLE · [11-31] RLE · [32-63] ONE_VALUE |

## uniform — random *(control)*

### 32-bit

| Codec | Ratio | Encode ms | Decode ms | Gather ms | Point ms | Sub-encodings |
|---|---|---|---|---|---|---|
| Uncompressed *(reference)* | 1.00 | 395 | 0.49 | 0.34 | 0.35 | — |
| ─────────────── | ─── | ─── | ─── | ─── | ─── | ─── |
| BP | 1.00 | 389 | 0.45 | 0.32 | 0.35 | — |
| PFOR | 1.00 | 385 | 0.43 | 0.32 | 0.36 | — |
| DICT | 1.00 | 956 | 0.44 | 0.32 | 0.35 | — |
| RLE | 1.00 | 893 | 0.43 | 0.33 | 0.32 | — |
| SubIntSplit (fixed split) | 1.00 | 1,418 | 0.37 | 0.30 | 0.32 | — |
| SubIntSplit (planned) | 1.00 | 6,545 | 0.42 | 0.30 | 0.38 | [0-31] UNCOMPRESSED |
| ─────────────── | ─── | ─── | ─── | ─── | ─── | ─── |
| BtrBlocks (auto, without SIS) | 1.00 | 520 | 0.49 | 0.36 | 0.42 | — |
| BtrBlocks (auto, with SIS) | 1.00 | 2,279 | 0.52 | 0.36 | 0.41 | — |

*Nothing is marked: no codec achieved compression on this dataset, so no result here is a win.*

### 64-bit

| Codec | Ratio | Encode ms | Decode ms | Gather ms | Point ms | Sub-encodings |
|---|---|---|---|---|---|---|
| Uncompressed *(reference)* | 1.00 | 1 | 0.78 | 0.02 | 0.00 | — |
| ─────────────── | ─── | ─── | ─── | ─── | ─── | ─── |
| SubIntSplit (fixed split) | 1.00 | 889 | 2.20 | 0.61 | 0.55 | [0-31] UNCOMPRESSED · [32-63] UNCOMPRESSED |
| SubIntSplit (planned) | 1.00 | 29,568 | 1.65 | 0.23 | 0.31 | [0-31] UNCOMPRESSED · [32-63] UNCOMPRESSED |

*Nothing is marked: no codec achieved compression on this dataset, so no result here is a win.*
