# SOAPdenovo-Trans2

## RAM optimization for SOAPdenovo-Trans

## How to apply these changes:

The modified are provided in the `src` directory. To apply the changes, simply overwrite the original files on the SOAPdenovo-Trans directories that will be downloaded alongside this repo. The original folder structure was kept in order to simplify the overload of the new code. When loading the new script, only overload each file and not the complete directory since the rest of the files are necessary to the correct execution of the script.

Here is a list of the files to overwrite:

1. `src/inc/newhash.h`
2. `src/newhash.c`
3. `src/hashFunction.c`
4. `src/prlHashReads.c`
5. `src/prlRead2path.c`
6. `src/localAsm.c`
7. `src/prlHashCtg.c`
8. `src/Makefile`

## How to compile and run it:

To compile and run this script, just follow the instructions given in the `aquaskyline/SOAPdenovo-Trans` repo.

To compile, open the SOAPdenovo-Trans-master folder in the terminal:

<pre>

sh make.sh

</pre>

To execute, open the SOAPdenovo-Trans-master folder in the terminal:

<pre>

./SOAPdenovo-Trans all -s config_file -o output_prefix

</pre>



## Implemented Changes:

Conservative optimizations were applied to the de Bruijn graph construction/mapping pipeline. The topology and logical graph model (`KmerSet` → nodes → `preEDGE`/`EDGE` → `ARC`) were not modified.

### 1. Correction and centralization of the hash load factor
**Files:** `src/inc/newhash.h`, `src/localAsm.c`, `src/prlHashCtg.c`, `src/prlHashReads.c`, `src/node2edge.c` (the latter already used `K_LOAD_FACTOR`).

- `K_LOAD_FACTOR`: `0.75` → `0.80`.
- Removed the hard-coded `0.77f` values from the main `KmerSet` creation sites.
- In `init_kmerset()`, the load factor is validated **before** calculating `set->max`.

**Effect:** fewer unused slots for the same number of k-mers, with a moderate trade-off in the number of collisions. The value remains configurable through `K_LOAD_FACTOR`.

### 2. Initial sizing of KmerSet shards
**File:** `src/prlHashReads.c`.

Previously, every `KmerSet` started with 1024 entries, causing numerous resizes/re-hashes while processing large datasets. In addition, the `-a` branch used a variable `k` initialized to zero, which could result in a minimal initial size rather than the requested one.

Now:

- without `-a`: initial capacity per shard ≈ `buffer_size / thrd_num`;
- with `-a`: the budget is interpreted as the total initial capacity and divided among the shards;
- minimum: 1024 entries/shard.

**Effect:** drastically fewer resizes during the hashing phase and memory initialization that is independent of the thread count for the same total budget.

### 3. More robust hash-table growth
**File:** `src/newhash.c`.

- `realloc()` now uses a temporary pointer, avoiding loss of the original pointer if allocation fails.
- Allocation checks were added for arrays and flags.
- Growth behavior remains based on doubling the capacity followed by the next prime number, so the probing strategy/topology is unchanged.

**Effect:** same hash-table algorithm, but lower risk of corruption/loss of the structure in case of OOM and less resize overhead thanks to the improved initial sizing.

### 4. Faster k-mer hashing
**File:** `src/hashFunction.c`.

The previous implementation computed a byte-by-byte CRC32 over the entire `Kmer` using a 256-entry table.

It was replaced with a 64-bit mixing function specialized for `MER31`, `MER63`, and `MER127`, while preserving:

- the same `hash_kmer(Kmer)` API;
- the same final 24-bit hash space (`KMER_HASH_MASK`);
- the same usage for sharding/probing.

**Expected effect:** lower CPU cost for hash computation, which is on the hot path of `put_kmerset()`/`search_kmerset()`.

Note: the numerical hash values change, but they are not part of the semantic graph output; they are only used for internal partitioning and probing.

### 5. Reduction of peak memory usage from k-mer buffers
**Files:** `src/prlHashReads.c`, `src/prlRead2path.c`.

The static buffer of `100000000` entries was reduced to `8000000` entries by default.

A different size can also be selected without recompiling:

```bash
SOAP_KMER_BUFFER_SIZE=16000000 ./SOAPdenovo-Trans-...
```

Accepted range: 1024 .. 1,000,000,000 entries.

**Estimated size of the main buffers only:**

| Mode | Before, hash reads | After, hash reads | Before, read→edge | After, read→edge |
|---|---:|---:|---:|---:|
| MER31 | ~1.8 GB | ~144 MB | ~3.4 GB | ~272 MB |
| MER63 | ~2.6 GB | ~208 MB | ~5.0 GB | ~400 MB |
| MER127 | ~4.2 GB | ~336 MB | ~8.2 GB | ~656 MB |

These are theoretical estimates for entry-indexed buffers and do not represent the total process RSS.

**Effect:** strong reduction in peak RAM before processing the first batch. The trade-off is a larger number of batches/I/O iterations compared with 100M entries; the value is therefore configurable.

### 6. KmerSet memory release: lifetime verification
**Files analyzed:** `src/pregraph.c`, `src/node2edge.c`, `src/prlRead2path.c`, `src/output_pregraph.c`.

No arbitrary early `free_Sets()` was introduced: `KmerSets` are still required by `kmer2edges()` and `output_vertex()`, while `KmerSetsPatch` is still used by `prlRead2edge()`.

The existing release in `pregraph.c` therefore remains at the correct point:

```c
output_vertex(graphfile);
free_Sets(KmerSets, thrd_num);
free_Sets(KmerSetsPatch, thrd_num);
```

**Rationale:** freeing these structures earlier would introduce a use-after-free. This part was therefore left unchanged to preserve correctness.

## Modified Files

1. `src/inc/newhash.h` — global load factor.
2. `src/newhash.c` — safe `KmerSet` initialization/resizing.
3. `src/hashFunction.c` — new fast hash function.
4. `src/prlHashReads.c` — initial shard sizing + configurable buffer.
5. `src/prlRead2path.c` — configurable buffer for the read→edge phase.
6. `src/localAsm.c` — use of `K_LOAD_FACTOR`.
7. `src/prlHashCtg.c` — use of `K_LOAD_FACTOR`.

## Verification Performed

The six main modified files were subjected to syntax-only compilation with GCC for all three modes:

- `MER31`
- `MER63`
- `MER127`

All pass `gcc -fsyntax-only`.

A full build using the original `Makefile` was also attempted. The complete repository build does not reach linking because of errors already present in the original codebase, including missing declarations such as `output_pool`, `deleteWeakEdge`, `deleteUnlikeArc`, `deleteShortContig`, `print_kmer_gz`, `ScafStat`, `getReadOnScaf`, and `RPKMStat`. These errors are not located in the files modified by the optimizations.
