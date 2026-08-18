# Results at block_size = 4,096

Ratio is raw bytes over encoded bytes, so higher is better; every time is milliseconds,
lower better. **Bold marks the winner of a column.**

The uncompressed reference and any codec that compressed essentially nothing (ratio ≤ 1.01) are shown but never marked as winners — otherwise a codec that fell back to storing the data verbatim would win the speed columns for doing no work.

Gather uses the *clustered* trace. Uniform gather touches every chunk once the request count exceeds the chunk count, so it degenerates into a full column decode for every codec and separates nothing.

## tweet_ids — real Twitter identifiers

### 64-bit

| Codec | Ratio | Encode ms | Decode ms | Gather ms | Point ms | Sub-encodings |
|---|---|---|---|---|---|---|
| Uncompressed *(reference)* | 1.00 | 240 | 1.24 | 0.35 | 0.38 | — |
| ─────────────── | ─── | ─── | ─── | ─── | ─── | ─── |
| BP | 1.09 | 1,099 | 2.69 | 0.74 | 0.65 | section:UNCOMPRESSED, section:BP |
| FOR | 1.11 | 1,403 | 2.65 | 0.76 | 0.71 | biased:BP, section:UNCOMPRESSED, section:BP |
| DICT | 1.00 | 565 | 1.44 | 0.43 | 0.46 | — |
| RLE | 1.09 | 1,435 | 4.49 | 1.94 | 1.45 | values:BP, section:UNCOMPRESSED, section:BP, counts:RLE, values:UNCOMPRESSED, counts:UNCOMPRESSED |
| SubIntSplit (fixed split) | 1.09 | **1,094** | 2.57 | 0.64 | **0.56** | [0-31] UNCOMPRESSED · [32-63] BP |
| SubIntSplit (planned) | **1.57** | 23,965 | 6.69 | 2.05 | 1.26 | [0-3] BP · [4-11] BP · [12-16] DICT · [17-21] RLE · [22-48] BP · [49-53] RLE · [54-63] RLE |
| ─────────────── | ─── | ─── | ─── | ─── | ─── | ─── |
| BtrBlocks (auto, without SIS) | 1.09 | 1,233 | **2.33** | **0.63** | 0.60 | section:UNCOMPRESSED, section:BP |
| BtrBlocks (auto, with SIS) | 1.56 | 35,186 | 7.70 | 2.31 | 1.38 | [0-3] BP · [4-11] BP · [12-16] DICT · [17-21] DICT · [22-48] BP · [49-53] RLE · [54-63] RLE |

## snowflake — generated, dense burst

### 32-bit

| Codec | Ratio | Encode ms | Decode ms | Gather ms | Point ms | Sub-encodings |
|---|---|---|---|---|---|---|
| Uncompressed *(reference)* | 1.00 | 271 | 0.44 | 0.35 | 0.35 | — |
| ─────────────── | ─── | ─── | ─── | ─── | ─── | ─── |
| BP | 1.11 | **298** | 0.98 | 0.56 | 0.48 | — |
| PFOR | 1.12 | 306 | 1.11 | 0.72 | 1.27 | — |
| DICT | 1.00 | 670 | 0.46 | 0.35 | 0.39 | — |
| RLE | 1.11 | 682 | 3.00 | 1.88 | 1.44 | values:BP, counts:RLE, values:UNCOMPRESSED, counts:UNCOMPRESSED |
| SubIntSplit (fixed split) | 2.06 | 941 | 1.25 | 0.72 | 0.55 | [0-15] BP · [16-31] RLE |
| SubIntSplit (planned) | **4.03** | 5,915 | 1.96 | 1.52 | 0.90 | [0-10] BP · [11-14] RLE · [15-31] RLE |
| ─────────────── | ─── | ─── | ─── | ─── | ─── | ─── |
| BtrBlocks (auto, without SIS) | 1.11 | 365 | **0.93** | **0.55** | **0.46** | — |
| BtrBlocks (auto, with SIS) | **4.03** | 7,329 | 1.82 | 1.29 | 0.75 | [0-10] BP · [11-14] RLE · [15-31] RLE |

### 64-bit

| Codec | Ratio | Encode ms | Decode ms | Gather ms | Point ms | Sub-encodings |
|---|---|---|---|---|---|---|
| Uncompressed *(reference)* | 1.00 | 480 | 2.17 | 0.63 | 0.75 | — |
| ─────────────── | ─── | ─── | ─── | ─── | ─── | ─── |
| BP | 2.05 | 1,229 | **1.83** | **0.51** | 0.48 | section:BP, section:RLE, values:UNCOMPRESSED, counts:UNCOMPRESSED |
| FOR | 2.86 | 1,739 | 3.39 | 0.90 | 0.91 | biased:BP, section:BP, section:RLE, values:UNCOMPRESSED, counts:UNCOMPRESSED |
| DICT | 1.00 | 1,180 | 2.19 | 0.69 | 0.65 | — |
| RLE | 2.04 | 1,883 | 6.11 | 2.80 | 1.91 | values:BP, section:BP, section:RLE, values:UNCOMPRESSED, counts:UNCOMPRESSED, counts:RLE, values:UNCOMPRESSED, counts:UNCOMPRESSED |
| SubIntSplit (fixed split) | 2.05 | **853** | 2.35 | 0.55 | **0.47** | [0-31] BP · [32-63] ONE_VALUE |
| SubIntSplit (planned) | **6.91** | 24,333 | 4.99 | 1.61 | 0.88 | [0-3] BP · [4-9] RLE · [10-22] BP · [23-31] RLE · [32-63] ONE_VALUE |
| ─────────────── | ─── | ─── | ─── | ─── | ─── | ─── |
| BtrBlocks (auto, without SIS) | 2.05 | 1,383 | 2.75 | 0.82 | 0.59 | section:BP, section:RLE, values:UNCOMPRESSED, counts:UNCOMPRESSED |
| BtrBlocks (auto, with SIS) | **6.91** | 24,194 | 4.26 | 1.43 | 0.74 | [0-3] BP · [4-9] RLE · [10-22] BP · [23-31] RLE · [32-63] ONE_VALUE |

## increasing — monotone, small steps

### 32-bit

| Codec | Ratio | Encode ms | Decode ms | Gather ms | Point ms | Sub-encodings |
|---|---|---|---|---|---|---|
| Uncompressed *(reference)* | 1.00 | 186 | 0.83 | 0.59 | 0.54 | — |
| ─────────────── | ─── | ─── | ─── | ─── | ─── | ─── |
| BP | 1.43 | **183** | 1.36 | 1.04 | 0.80 | — |
| PFOR | 1.44 | 222 | 1.23 | 0.76 | 1.32 | — |
| DICT | 1.00 | 574 | 0.68 | 0.57 | 0.71 | — |
| RLE | 1.42 | 515 | 4.50 | 2.77 | 2.57 | values:BP, counts:RLE, values:UNCOMPRESSED, counts:UNCOMPRESSED |
| SubIntSplit (fixed split) | 2.08 | 494 | 1.61 | 1.18 | 0.78 | [0-15] BP · [16-31] ONE_VALUE |
| SubIntSplit (planned) | **4.08** | 7,588 | 2.04 | 1.23 | 0.79 | [0-5] BP · [6-10] RLE · [11-31] RLE |
| ─────────────── | ─── | ─── | ─── | ─── | ─── | ─── |
| BtrBlocks (auto, without SIS) | 1.43 | 289 | **0.87** | **0.56** | **0.45** | — |
| BtrBlocks (auto, with SIS) | **4.08** | 8,890 | 3.18 | 2.28 | 1.43 | [0-5] BP · [6-10] RLE · [11-31] RLE |

### 64-bit

| Codec | Ratio | Encode ms | Decode ms | Gather ms | Point ms | Sub-encodings |
|---|---|---|---|---|---|---|
| Uncompressed *(reference)* | 1.00 | 140 | 1.19 | 0.34 | 0.37 | — |
| ─────────────── | ─── | ─── | ─── | ─── | ─── | ─── |
| BP | 2.85 | **390** | **1.93** | **0.62** | 0.53 | section:BP, section:RLE, values:UNCOMPRESSED, counts:UNCOMPRESSED |
| FOR | 4.70 | 592 | 2.05 | 0.64 | **0.52** | biased:BP, section:BP, section:RLE, values:UNCOMPRESSED, counts:UNCOMPRESSED |
| DICT | 1.00 | 388 | 1.25 | 0.36 | 0.42 | — |
| RLE | 2.84 | 639 | 4.17 | 1.95 | 1.41 | values:BP, section:BP, section:RLE, values:UNCOMPRESSED, counts:UNCOMPRESSED, counts:RLE, values:UNCOMPRESSED, counts:UNCOMPRESSED |
| SubIntSplit (fixed split) | 2.85 | 553 | 2.75 | 0.78 | 0.64 | [0-31] BP · [32-63] ONE_VALUE |
| SubIntSplit (planned) | **8.14** | 16,570 | 4.81 | 1.75 | 0.99 | [0-5] BP · [6-10] RLE · [11-31] RLE · [32-63] ONE_VALUE |
| ─────────────── | ─── | ─── | ─── | ─── | ─── | ─── |
| BtrBlocks (auto, without SIS) | 2.85 | 475 | 3.41 | 1.02 | 0.84 | section:BP, section:RLE, values:UNCOMPRESSED, counts:UNCOMPRESSED |
| BtrBlocks (auto, with SIS) | **8.14** | 22,880 | 5.21 | 1.92 | 1.26 | [0-5] BP · [6-10] RLE · [11-31] RLE · [32-63] ONE_VALUE |

## uniform — random *(control)*

### 32-bit

| Codec | Ratio | Encode ms | Decode ms | Gather ms | Point ms | Sub-encodings |
|---|---|---|---|---|---|---|
| Uncompressed *(reference)* | 1.00 | 421 | 0.39 | 0.32 | 0.38 | — |
| ─────────────── | ─── | ─── | ─── | ─── | ─── | ─── |
| BP | 1.00 | 417 | 0.39 | 0.33 | 0.37 | — |
| PFOR | 1.00 | 429 | 0.42 | 0.39 | 0.44 | — |
| DICT | 1.00 | 978 | 0.39 | 0.32 | 0.35 | — |
| RLE | 1.00 | 921 | 0.37 | 0.32 | 0.36 | — |
| SubIntSplit (fixed split) | 1.00 | 1,486 | 0.39 | 0.32 | 0.39 | — |
| SubIntSplit (planned) | 1.00 | 7,526 | 0.86 | 0.46 | 0.56 | [0-31] UNCOMPRESSED |
| ─────────────── | ─── | ─── | ─── | ─── | ─── | ─── |
| BtrBlocks (auto, without SIS) | 1.00 | 495 | 0.48 | 0.45 | 0.51 | — |
| BtrBlocks (auto, with SIS) | 1.00 | 2,422 | 0.40 | 0.32 | 0.34 | — |

*Nothing is marked: no codec achieved compression on this dataset, so no result here is a win.*

### 64-bit

| Codec | Ratio | Encode ms | Decode ms | Gather ms | Point ms | Sub-encodings |
|---|---|---|---|---|---|---|
| Uncompressed *(reference)* | 1.00 | 434 | 1.23 | 0.35 | 0.40 | — |
| ─────────────── | ─── | ─── | ─── | ─── | ─── | ─── |
| BP | 1.00 | 1,586 | 1.15 | 0.34 | 0.33 | — |
| FOR | 1.00 | 1,119 | 1.33 | 0.38 | 0.43 | — |
| DICT | 1.00 | 1,122 | 1.71 | 0.46 | 0.48 | — |
| RLE | 1.00 | 1,089 | 1.40 | 0.38 | 0.45 | — |
| SubIntSplit (fixed split) | 1.00 | 1,435 | 1.27 | 0.35 | 0.41 | [0-31] UNCOMPRESSED · [32-63] UNCOMPRESSED |
| SubIntSplit (planned) | 1.00 | 25,128 | 1.22 | 0.36 | 0.41 | [0-31] UNCOMPRESSED · [32-63] UNCOMPRESSED |
| ─────────────── | ─── | ─── | ─── | ─── | ─── | ─── |
| BtrBlocks (auto, without SIS) | 1.00 | 678 | 1.16 | 0.34 | 0.39 | — |
| BtrBlocks (auto, with SIS) | 1.00 | 8,805 | 1.18 | 0.35 | 0.39 | — |

*Nothing is marked: no codec achieved compression on this dataset, so no result here is a win.*
