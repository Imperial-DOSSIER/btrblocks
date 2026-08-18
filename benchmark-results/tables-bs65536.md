# Results at block_size = 65,536

Ratio is raw bytes over encoded bytes, so higher is better; every time is milliseconds,
lower better. **Bold marks the winner of a column.**

The uncompressed reference and any codec that compressed essentially nothing (ratio ≤ 1.01) are shown but never marked as winners — otherwise a codec that fell back to storing the data verbatim would win the speed columns for doing no work.

Gather uses the *clustered* trace. Uniform gather touches every chunk once the request count exceeds the chunk count, so it degenerates into a full column decode for every codec and separates nothing.

## tweet_ids — real Twitter identifiers

### 64-bit

| Codec | Ratio | Encode ms | Decode ms | Gather ms | Point ms | Sub-encodings |
|---|---|---|---|---|---|---|
| Uncompressed *(reference)* | 1.00 | 276 | 1.13 | 0.10 | 0.07 | — |
| ─────────────── | ─── | ─── | ─── | ─── | ─── | ─── |
| BP | 1.09 | **1,026** | 2.04 | **0.71** | 0.86 | section:UNCOMPRESSED, section:BP |
| FOR | 1.09 | 1,301 | 2.36 | 0.73 | 0.87 | biased:BP, section:UNCOMPRESSED, section:BP |
| DICT | 1.00 | 649 | 1.11 | 0.10 | 0.07 | — |
| RLE | 1.09 | 1,287 | 4.09 | 2.32 | 11.13 | values:BP, section:UNCOMPRESSED, section:BP, counts:RLE, values:UNCOMPRESSED, counts:UNCOMPRESSED |
| SubIntSplit (fixed split) | 1.09 | 1,060 | 2.13 | 0.73 | 0.90 | [0-31] UNCOMPRESSED · [32-63] BP |
| SubIntSplit (planned) | 1.54 | 2,343 | 8.15 | 3.67 | 4.71 | [0-2] BP · [3-11] BP · [12-17] BP · [18-21] RLE · [22-50] BP · [51-55] RLE · [56-63] RLE |
| ─────────────── | ─── | ─── | ─── | ─── | ─── | ─── |
| BtrBlocks (auto, without SIS) | 1.09 | 1,026 | **2.02** | 0.73 | **0.57** | section:UNCOMPRESSED, section:BP |
| BtrBlocks (auto, with SIS) | **1.54** | 2,693 | 7.05 | 3.75 | 5.36 | [0-2] BP · [3-11] BP · [12-17] BP · [18-21] RLE · [22-50] BP · [51-55] RLE · [56-63] RLE |

## snowflake — generated, dense burst

### 32-bit

| Codec | Ratio | Encode ms | Decode ms | Gather ms | Point ms | Sub-encodings |
|---|---|---|---|---|---|---|
| Uncompressed *(reference)* | 1.00 | 215 | 0.54 | 0.11 | 0.07 | — |
| ─────────────── | ─── | ─── | ─── | ─── | ─── | ─── |
| BP | 1.11 | **217** | 0.83 | 0.64 | 0.48 | — |
| PFOR | 1.12 | 220 | **0.49** | **0.34** | 3.73 | — |
| DICT | 1.00 | 521 | 0.43 | 0.09 | 0.05 | — |
| RLE | 1.11 | 436 | 2.49 | 2.20 | 10.90 | values:BP, counts:RLE, values:UNCOMPRESSED, counts:UNCOMPRESSED |
| SubIntSplit (fixed split) | 2.63 | 743 | 1.67 | 0.91 | 0.65 | [0-15] DICT · [16-31] RLE |
| SubIntSplit (planned) | **4.11** | 672 | 1.74 | 1.24 | 1.25 | [0-10] DICT · [11-18] RLE · [19-31] RLE |
| ─────────────── | ─── | ─── | ─── | ─── | ─── | ─── |
| BtrBlocks (auto, without SIS) | 1.11 | 220 | 0.80 | 0.64 | **0.48** | — |
| BtrBlocks (auto, with SIS) | **4.11** | 745 | 1.73 | 1.23 | 1.27 | [0-10] DICT · [11-18] RLE · [19-31] RLE |

### 64-bit

| Codec | Ratio | Encode ms | Decode ms | Gather ms | Point ms | Sub-encodings |
|---|---|---|---|---|---|---|
| Uncompressed *(reference)* | 1.00 | 295 | 1.07 | 0.10 | 0.06 | — |
| ─────────────── | ─── | ─── | ─── | ─── | ─── | ─── |
| BP | 2.06 | 599 | **1.69** | 0.40 | 0.26 | section:BP, section:RLE, values:UNCOMPRESSED, counts:UNCOMPRESSED |
| FOR | 2.27 | 918 | 2.27 | 0.71 | 0.54 | biased:BP, section:BP, section:RLE, values:UNCOMPRESSED, counts:UNCOMPRESSED |
| DICT | 1.00 | 692 | 1.05 | 0.10 | 0.05 | — |
| RLE | 2.06 | 936 | 3.68 | 1.97 | 10.71 | values:BP, section:BP, section:RLE, values:UNCOMPRESSED, counts:UNCOMPRESSED, counts:RLE, values:UNCOMPRESSED, counts:UNCOMPRESSED |
| SubIntSplit (fixed split) | 2.06 | 582 | 1.89 | **0.38** | **0.26** | [0-31] BP · [32-63] ONE_VALUE |
| SubIntSplit (planned) | **6.29** | 1,363 | 4.40 | 1.83 | 1.87 | [0-3] BP · [4-9] RLE · [10-24] DICT · [25-31] RLE · [32-63] ONE_VALUE |
| ─────────────── | ─── | ─── | ─── | ─── | ─── | ─── |
| BtrBlocks (auto, without SIS) | 2.06 | **581** | 1.72 | 0.43 | 0.29 | section:BP, section:RLE, values:UNCOMPRESSED, counts:UNCOMPRESSED |
| BtrBlocks (auto, with SIS) | **6.29** | 1,596 | 4.44 | 1.82 | 1.87 | [0-3] BP · [4-9] RLE · [10-24] DICT · [25-31] RLE · [32-63] ONE_VALUE |

## increasing — monotone, small steps

### 32-bit

| Codec | Ratio | Encode ms | Decode ms | Gather ms | Point ms | Sub-encodings |
|---|---|---|---|---|---|---|
| Uncompressed *(reference)* | 1.00 | 128 | 0.42 | 0.09 | 0.05 | — |
| ─────────────── | ─── | ─── | ─── | ─── | ─── | ─── |
| BP | 1.43 | **129** | 0.77 | 0.65 | 0.49 | — |
| PFOR | 1.44 | 133 | **0.47** | **0.33** | 3.58 | — |
| DICT | 1.00 | 363 | 0.45 | 0.10 | 0.05 | — |
| RLE | 1.43 | 282 | 2.47 | 2.41 | 10.91 | values:BP, counts:RLE, values:UNCOMPRESSED, counts:UNCOMPRESSED |
| SubIntSplit (fixed split) | 2.09 | 389 | 1.08 | 0.73 | 0.56 | [0-15] BP · [16-31] RLE |
| SubIntSplit (planned) | **4.29** | 469 | 1.73 | 1.80 | 2.29 | [0-5] BP · [6-10] RLE · [11-31] RLE |
| ─────────────── | ─── | ─── | ─── | ─── | ─── | ─── |
| BtrBlocks (auto, without SIS) | 1.43 | 132 | 0.77 | 0.65 | **0.48** | — |
| BtrBlocks (auto, with SIS) | **4.29** | 530 | 1.81 | 1.47 | 1.85 | [0-5] BP · [6-10] RLE · [11-31] RLE |

### 64-bit

| Codec | Ratio | Encode ms | Decode ms | Gather ms | Point ms | Sub-encodings |
|---|---|---|---|---|---|---|
| Uncompressed *(reference)* | 1.00 | 179 | 1.12 | 0.10 | 0.06 | — |
| ─────────────── | ─── | ─── | ─── | ─── | ─── | ─── |
| BP | 2.87 | **336** | 2.13 | 0.68 | 0.51 | section:BP, section:RLE, values:UNCOMPRESSED, counts:UNCOMPRESSED |
| FOR | 3.66 | 630 | 2.33 | 0.86 | 0.63 | biased:BP, section:BP, section:RLE, values:UNCOMPRESSED, counts:UNCOMPRESSED |
| DICT | 1.00 | 428 | 1.10 | 0.10 | 0.05 | — |
| RLE | 2.86 | 676 | 3.75 | 2.25 | 10.94 | values:BP, section:BP, section:RLE, values:UNCOMPRESSED, counts:UNCOMPRESSED, counts:RLE, values:UNCOMPRESSED, counts:UNCOMPRESSED |
| SubIntSplit (fixed split) | 2.87 | 366 | 1.92 | **0.66** | **0.50** | [0-31] BP · [32-63] ONE_VALUE |
| SubIntSplit (planned) | **8.58** | 1,106 | 2.90 | 1.49 | 1.88 | [0-5] BP · [6-10] RLE · [11-31] RLE · [32-63] ONE_VALUE |
| ─────────────── | ─── | ─── | ─── | ─── | ─── | ─── |
| BtrBlocks (auto, without SIS) | 2.87 | 341 | **1.65** | 0.69 | 0.50 | section:BP, section:RLE, values:UNCOMPRESSED, counts:UNCOMPRESSED |
| BtrBlocks (auto, with SIS) | **8.58** | 1,336 | 3.21 | 1.47 | 1.87 | [0-5] BP · [6-10] RLE · [11-31] RLE · [32-63] ONE_VALUE |

## uniform — random *(control)*

### 32-bit

| Codec | Ratio | Encode ms | Decode ms | Gather ms | Point ms | Sub-encodings |
|---|---|---|---|---|---|---|
| Uncompressed *(reference)* | 1.00 | 512 | 0.43 | 0.09 | 0.05 | — |
| ─────────────── | ─── | ─── | ─── | ─── | ─── | ─── |
| BP | 1.00 | 504 | 0.44 | 0.10 | 0.05 | — |
| PFOR | 1.00 | 503 | 0.43 | 0.09 | 0.05 | — |
| DICT | 1.00 | 1,205 | 0.44 | 0.10 | 0.05 | — |
| RLE | 1.00 | 1,012 | 0.43 | 0.10 | 0.05 | — |
| SubIntSplit (fixed split) | 1.00 | 1,390 | 0.43 | 0.10 | 0.06 | — |
| SubIntSplit (planned) | 1.00 | 1,291 | 0.43 | 0.09 | 0.05 | [0-31] UNCOMPRESSED |
| ─────────────── | ─── | ─── | ─── | ─── | ─── | ─── |
| BtrBlocks (auto, without SIS) | 1.00 | 510 | 0.43 | 0.09 | 0.05 | — |
| BtrBlocks (auto, with SIS) | 1.00 | 606 | 0.42 | 0.09 | 0.05 | — |

*Nothing is marked: no codec achieved compression on this dataset, so no result here is a win.*

### 64-bit

| Codec | Ratio | Encode ms | Decode ms | Gather ms | Point ms | Sub-encodings |
|---|---|---|---|---|---|---|
| Uncompressed *(reference)* | 1.00 | 527 | 1.12 | 0.10 | 0.05 | — |
| ─────────────── | ─── | ─── | ─── | ─── | ─── | ─── |
| BP | 1.00 | 1,562 | 1.12 | 0.10 | 0.05 | — |
| FOR | 1.00 | 1,063 | 1.12 | 0.10 | 0.06 | — |
| DICT | 1.00 | 1,257 | 1.14 | 0.10 | 0.05 | — |
| RLE | 1.00 | 1,070 | 1.10 | 0.10 | 0.05 | — |
| SubIntSplit (fixed split) | 1.00 | 2,038 | 2.15 | 0.14 | 0.08 | [0-31] UNCOMPRESSED · [32-63] UNCOMPRESSED |
| SubIntSplit (planned) | 1.00 | 2,754 | 1.12 | 0.10 | 0.05 | [0-31] UNCOMPRESSED · [32-63] UNCOMPRESSED |
| ─────────────── | ─── | ─── | ─── | ─── | ─── | ─── |
| BtrBlocks (auto, without SIS) | 1.00 | 552 | 1.05 | 0.10 | 0.06 | — |
| BtrBlocks (auto, with SIS) | 1.00 | 1,067 | 1.62 | 0.15 | 0.09 | — |

*Nothing is marked: no codec achieved compression on this dataset, so no result here is a win.*
