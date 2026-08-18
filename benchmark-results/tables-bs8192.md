# Results at block_size = 8,192

Ratio is raw bytes over encoded bytes, so higher is better; every time is milliseconds,
lower better. **Bold marks the winner of a column.**

The uncompressed reference and any codec that compressed essentially nothing (ratio ≤ 1.01) are shown but never marked as winners — otherwise a codec that fell back to storing the data verbatim would win the speed columns for doing no work.

Gather uses the *clustered* trace. Uniform gather touches every chunk once the request count exceeds the chunk count, so it degenerates into a full column decode for every codec and separates nothing.

## tweet_ids — real Twitter identifiers

### 64-bit

| Codec | Ratio | Encode ms | Decode ms | Gather ms | Point ms | Sub-encodings |
|---|---|---|---|---|---|---|
| Uncompressed *(reference)* | 1.00 | 201 | 1.19 | 0.28 | 0.24 | — |
| ─────────────── | ─── | ─── | ─── | ─── | ─── | ─── |
| BP | 1.09 | 868 | 1.76 | 0.49 | 0.35 | section:UNCOMPRESSED, section:BP |
| FOR | 1.10 | 1,081 | 2.10 | 0.49 | 0.36 | biased:BP, section:UNCOMPRESSED, section:BP |
| DICT | 1.00 | 495 | 0.98 | 0.23 | 0.20 | — |
| RLE | 1.09 | 1,133 | 4.63 | 2.30 | 2.05 | values:BP, section:UNCOMPRESSED, section:BP, counts:RLE, values:UNCOMPRESSED, counts:UNCOMPRESSED |
| SubIntSplit (fixed split) | 1.09 | **864** | 2.05 | 0.50 | 0.38 | [0-31] UNCOMPRESSED · [32-63] BP |
| SubIntSplit (planned) | 1.56 | 12,445 | 6.63 | 2.19 | 1.43 | [0-3] BP · [4-11] BP · [12-17] DICT · [18-21] RLE · [22-49] BP · [50-54] RLE · [55-63] RLE |
| ─────────────── | ─── | ─── | ─── | ─── | ─── | ─── |
| BtrBlocks (auto, without SIS) | 1.09 | 915 | **1.76** | **0.49** | **0.34** | section:UNCOMPRESSED, section:BP |
| BtrBlocks (auto, with SIS) | **1.56** | 15,262 | 6.68 | 2.20 | 1.41 | [0-3] BP · [4-11] BP · [12-17] DICT · [18-21] RLE · [22-49] BP · [50-54] RLE · [55-63] RLE |

## snowflake — generated, dense burst

### 32-bit

| Codec | Ratio | Encode ms | Decode ms | Gather ms | Point ms | Sub-encodings |
|---|---|---|---|---|---|---|
| Uncompressed *(reference)* | 1.00 | 213 | 0.41 | 0.22 | 0.18 | — |
| ─────────────── | ─── | ─── | ─── | ─── | ─── | ─── |
| BP | 1.11 | 246 | 0.80 | **0.43** | **0.30** | — |
| PFOR | 1.12 | **233** | **0.79** | 0.63 | 1.31 | — |
| DICT | 1.00 | 519 | 0.42 | 0.22 | 0.19 | — |
| RLE | 1.11 | 475 | 2.40 | 1.74 | 1.71 | values:BP, counts:RLE, values:UNCOMPRESSED, counts:UNCOMPRESSED |
| SubIntSplit (fixed split) | 2.07 | 578 | 1.09 | 0.54 | 0.37 | [0-15] BP · [16-31] RLE |
| SubIntSplit (planned) | **4.11** | 2,454 | 1.54 | 1.00 | 0.52 | [0-10] BP · [11-15] RLE · [16-31] RLE |
| ─────────────── | ─── | ─── | ─── | ─── | ─── | ─── |
| BtrBlocks (auto, without SIS) | 1.11 | 260 | 0.87 | 0.48 | 0.32 | — |
| BtrBlocks (auto, with SIS) | **4.11** | 3,039 | 1.79 | 1.20 | 0.63 | [0-10] BP · [11-15] RLE · [16-31] RLE |

### 64-bit

| Codec | Ratio | Encode ms | Decode ms | Gather ms | Point ms | Sub-encodings |
|---|---|---|---|---|---|---|
| Uncompressed *(reference)* | 1.00 | 252 | 0.98 | 0.24 | 0.19 | — |
| ─────────────── | ─── | ─── | ─── | ─── | ─── | ─── |
| BP | 2.05 | 638 | 1.39 | 0.39 | 0.30 | section:BP, section:RLE, values:UNCOMPRESSED, counts:UNCOMPRESSED |
| FOR | 2.63 | 900 | 1.90 | 0.51 | 0.36 | biased:BP, section:BP, section:RLE, values:UNCOMPRESSED, counts:UNCOMPRESSED |
| DICT | 1.00 | 638 | 0.98 | 0.23 | 0.20 | — |
| RLE | 2.05 | 914 | 3.23 | 1.66 | 1.56 | values:BP, section:BP, section:RLE, values:UNCOMPRESSED, counts:UNCOMPRESSED, counts:RLE, values:UNCOMPRESSED, counts:UNCOMPRESSED |
| SubIntSplit (fixed split) | 2.05 | **616** | 1.64 | **0.36** | **0.25** | [0-31] BP · [32-63] ONE_VALUE |
| SubIntSplit (planned) | **7.05** | 7,456 | 3.60 | 1.22 | 0.63 | [0-3] BP · [4-9] RLE · [10-22] BP · [23-31] RLE · [32-63] ONE_VALUE |
| ─────────────── | ─── | ─── | ─── | ─── | ─── | ─── |
| BtrBlocks (auto, without SIS) | 2.05 | 661 | **1.37** | 0.38 | 0.28 | section:BP, section:RLE, values:UNCOMPRESSED, counts:UNCOMPRESSED |
| BtrBlocks (auto, with SIS) | **7.05** | 9,271 | 3.60 | 1.24 | 0.60 | [0-3] BP · [4-9] RLE · [10-22] BP · [23-31] RLE · [32-63] ONE_VALUE |

## increasing — monotone, small steps

### 32-bit

| Codec | Ratio | Encode ms | Decode ms | Gather ms | Point ms | Sub-encodings |
|---|---|---|---|---|---|---|
| Uncompressed *(reference)* | 1.00 | 104 | 0.41 | 0.22 | 0.19 | — |
| ─────────────── | ─── | ─── | ─── | ─── | ─── | ─── |
| BP | 1.43 | **113** | 0.81 | **0.44** | **0.29** | — |
| PFOR | 1.44 | 118 | **0.66** | 0.55 | 1.11 | — |
| DICT | 1.00 | 297 | 0.41 | 0.22 | 0.20 | — |
| RLE | 1.43 | 245 | 2.39 | 1.75 | 1.70 | values:BP, counts:RLE, values:UNCOMPRESSED, counts:UNCOMPRESSED |
| SubIntSplit (fixed split) | 2.09 | 320 | 1.03 | 0.47 | 0.30 | [0-15] BP · [16-31] ONE_VALUE |
| SubIntSplit (planned) | **4.16** | 2,243 | 1.77 | 1.04 | 0.62 | [0-5] BP · [6-11] RLE · [12-31] RLE |
| ─────────────── | ─── | ─── | ─── | ─── | ─── | ─── |
| BtrBlocks (auto, without SIS) | 1.43 | 129 | 1.00 | 0.56 | 0.37 | — |
| BtrBlocks (auto, with SIS) | **4.16** | 2,758 | 1.71 | 1.05 | 0.61 | [0-5] BP · [6-11] RLE · [12-31] RLE |

### 64-bit

| Codec | Ratio | Encode ms | Decode ms | Gather ms | Point ms | Sub-encodings |
|---|---|---|---|---|---|---|
| Uncompressed *(reference)* | 1.00 | 125 | 1.10 | 0.25 | 0.22 | — |
| ─────────────── | ─── | ─── | ─── | ─── | ─── | ─── |
| BP | 2.86 | **302** | **1.50** | **0.48** | **0.32** | section:BP, section:RLE, values:UNCOMPRESSED, counts:UNCOMPRESSED |
| FOR | 4.40 | 430 | 2.87 | 0.69 | 0.47 | biased:BP, section:BP, section:RLE, values:UNCOMPRESSED, counts:UNCOMPRESSED |
| DICT | 1.00 | 660 | 1.34 | 0.34 | 0.29 | — |
| RLE | 2.85 | 579 | 3.37 | 1.76 | 1.58 | values:BP, section:BP, section:RLE, values:UNCOMPRESSED, counts:UNCOMPRESSED, counts:RLE, values:UNCOMPRESSED, counts:UNCOMPRESSED |
| SubIntSplit (fixed split) | 2.86 | 519 | 2.82 | 0.78 | 0.49 | [0-31] BP · [32-63] ONE_VALUE |
| SubIntSplit (planned) | **8.31** | 6,977 | 2.89 | 1.07 | 0.64 | [0-5] BP · [6-11] RLE · [12-31] RLE · [32-63] ONE_VALUE |
| ─────────────── | ─── | ─── | ─── | ─── | ─── | ─── |
| BtrBlocks (auto, without SIS) | 2.86 | 533 | 3.16 | 0.82 | 0.49 | section:BP, section:RLE, values:UNCOMPRESSED, counts:UNCOMPRESSED |
| BtrBlocks (auto, with SIS) | **8.31** | 13,252 | 5.67 | 1.95 | 1.41 | [0-5] BP · [6-11] RLE · [12-31] RLE · [32-63] ONE_VALUE |

## uniform — random *(control)*

### 32-bit

| Codec | Ratio | Encode ms | Decode ms | Gather ms | Point ms | Sub-encodings |
|---|---|---|---|---|---|---|
| Uncompressed *(reference)* | 1.00 | 356 | 0.41 | 0.23 | 0.19 | — |
| ─────────────── | ─── | ─── | ─── | ─── | ─── | ─── |
| BP | 1.00 | 362 | 0.43 | 0.23 | 0.18 | — |
| PFOR | 1.00 | 359 | 0.41 | 0.22 | 0.20 | — |
| DICT | 1.00 | 877 | 0.41 | 0.23 | 0.20 | — |
| RLE | 1.00 | 810 | 0.43 | 0.23 | 0.20 | — |
| SubIntSplit (fixed split) | 1.00 | 1,188 | 0.41 | 0.22 | 0.19 | — |
| SubIntSplit (planned) | 1.00 | 3,082 | 0.41 | 0.22 | 0.21 | [0-31] UNCOMPRESSED |
| ─────────────── | ─── | ─── | ─── | ─── | ─── | ─── |
| BtrBlocks (auto, without SIS) | 1.00 | 401 | 0.41 | 0.23 | 0.19 | — |
| BtrBlocks (auto, with SIS) | 1.00 | 1,195 | 0.51 | 0.27 | 0.25 | — |

*Nothing is marked: no codec achieved compression on this dataset, so no result here is a win.*

### 64-bit

| Codec | Ratio | Encode ms | Decode ms | Gather ms | Point ms | Sub-encodings |
|---|---|---|---|---|---|---|
| Uncompressed *(reference)* | 1.00 | 378 | 1.01 | 0.23 | 0.19 | — |
| ─────────────── | ─── | ─── | ─── | ─── | ─── | ─── |
| BP | 1.00 | 1,168 | 0.98 | 0.23 | 0.21 | — |
| FOR | 1.00 | 801 | 0.98 | 0.23 | 0.20 | — |
| DICT | 1.00 | 908 | 0.98 | 0.23 | 0.20 | — |
| RLE | 1.00 | 812 | 0.98 | 0.23 | 0.18 | — |
| SubIntSplit (fixed split) | 1.00 | 1,173 | 0.98 | 0.23 | 0.19 | [0-31] UNCOMPRESSED · [32-63] UNCOMPRESSED |
| SubIntSplit (planned) | 1.00 | 10,399 | 0.98 | 0.23 | 0.19 | [0-31] UNCOMPRESSED · [32-63] UNCOMPRESSED |
| ─────────────── | ─── | ─── | ─── | ─── | ─── | ─── |
| BtrBlocks (auto, without SIS) | 1.00 | 429 | 0.99 | 0.23 | 0.19 | — |
| BtrBlocks (auto, with SIS) | 1.00 | 3,437 | 0.98 | 0.23 | 0.19 | — |

*Nothing is marked: no codec achieved compression on this dataset, so no result here is a win.*
