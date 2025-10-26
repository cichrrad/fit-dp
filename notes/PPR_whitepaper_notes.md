# (AI-generated document)

---

# Notes for [Efficient Implementation of a Synchronous Parallel Push-Relabel Algorithm](./whitepapers/PPR.pdf)

# What this paper is (in one line)

A **synchronous, FIFO-friendly** parallel push–relabel (“**prsn**”) that batches work in rounds, minimizes atomics, uses a **deterministic neighbor-conflict rule**, and runs **periodic (not concurrent) parallel global relabeling**; on large sparse, low-diameter graphs it beats top sequential solvers by **up to 12×** and a strong parallel competitor by **≈3×** on 40 cores.

---

# Core approach (what’s different vs. standard PR)

* **Synchronous rounds over the active set.** Process *all* currently active vertices in parallel; each thread *discharges its vertex completely* (push/relabel loop) within the round. Effects are **buffered** and **committed at the end** (labels to `d` and excess to `e`). This is the coarse-grained sync that reduces contention. (Listing 1.1 lines 26–33, 68–73, 77–80.)

* **Deterministic neighbor-conflict rule (no locks).** If two adjacent vertices are active, only the **winner** may push across the shared edge this round:
  `v` wins if `d(v) < d(w)−1` **or** `d(v)=d(w)+1` **or** tie-break `v<w`. The loser skips that edge (and can abort if it was the last admissible). Relabels are written to a copy `d′` and committed at round end to preserve correctness. (Listing 1.1 lines 39–45, 56–61; text §3.1.)

* **Periodic, parallel global relabeling (not concurrent).** Use the **hi_pr budgeting**: trigger GR roughly every `≈ 12n + 2m` scanned-edge units (parameters `β=12`, `freq=0.5`, `α=6`). Implement GR as **reverse BFS from sink in parallel** with CAS for first-touch discovery. (Listing 1.1 lines 15–20; Listing 1.2 lines 2–15; text §3.)

* **FIFO-style evolution of the active set.** Next round’s working set is built from per-vertex “discovered” buffers (concat/prefix-sum), dropping `d(v)=n`. (Listing 1.1 lines 22, 65–76.) This matches the authors’ finding that **FIFO tends to beat highest-label** on their modern graphs and is more parallel-friendly.

* **What they *didn’t* keep:** no concurrent GR; **gap relabeling** gave no gains here. 

---

# Where the parallelism and atomics are (with exact pseudocode lines)

**Initialization (parallel):**

* Per-vertex zeroing (labels, excess, flags): **2–6**
* Per-edge zeroing of flow: **8–9**
* **Saturate all source-adjacent edges in parallel:** **11–14** 

**Round structure (parallel filter + trigger):**

* GR budget & call: **15–20**
* Filter working set (`d(v) < n`): **22**; empty check: **24** 

**Main work — discharge all active vertices (vertex-parallel):**

* Parallel over vertices: **26**; set up per-vertex temporaries: **27–33**
* **Edge scan inside a vertex’s discharge:** Listing shows “parallel foreach” at **34**, but the text specifies edges are **checked sequentially**; treat this as a **serial scan per vertex** (uses `break`).
* **Conflict rule:** **39–45**
* **Push path (non-atomic + atomics):**

  * Non-atomic updates to local variables/flow arrays: **49–51**
  * **Atomic fetch-and-add** to neighbor’s `addedExcess`: **53**
  * **Atomic TestAndSet** to mark `w.isDiscovered` (and enqueue once): **54–55**
  * Update tentative relabel `newLabel` and write to `d′(v)`: **56–61**
  * Buffer own residual `addedExcess`: **64**
  * Optional self-reactivation (TestAndSet): **65–66** 

**End-of-round commits (parallel):**

* Commit `d′→d`, apply `addedExcess`, clear flags: **68–73**
* Rebuild next working set (concat discovered lists, drop `d=n`): **74–76**
* Final flush of late `addedExcess` (parallel): **77–80** 

**Parallel Global Relabel (Listing 1.2):**

* Reset labels in parallel; set `d(t)=0`; init frontier: **2–5**
* **Parallel frontier expansion:** **7–9**
* **CAS for first discovery:** comment + branch **10–13**
* Prefix-sum concat to build next frontier: **15** 

**Summary of atomics only where needed:**

* **fetch-and-add** to `w.addedExcess` (**L1.1: 53**)
* **test-and-set** on `*.isDiscovered` (**L1.1: 54–55, 65–66**)
* **compare-and-swap** in GR BFS discovery (**L1.2: 10–13**)
  Everything else uses plain reads/writes within the round protocol.

---

# Why these choices help (in practice)

* Coarse-grained sync (batching) cuts down coherence traffic and contention; atomics are rare and well-placed; the deterministic rule avoids locks altogether while guaranteeing progress. Labels remain stable during pushes because **GR is periodic, not concurrent**. 

---

# Results & when to expect wins

* **Speedups:** up to **12× vs. best sequential** and **≈3× vs. a strong parallel baseline** on a 40-core Nehalem (E7-8870, 256 GiB). Hyper-threading didn’t help (memory-bandwidth bound).
* **Best on:** large, sparse, low-diameter graphs (e.g., RGG, Delaunay, nlpkkt240), and a web-scale spam-detection graph (pld_spam). 
* **Hard cases:** some road-like graphs (europe.osm) and certain RMF instances where parallelism is limited—sequential can still win. 

---

# Implementation checklist (prsn essentials)

1. **Data layout**

* Adjacency arrays; store capacity and the reverse edge’s residual capacity for quick updates. (Unifies implementations for fair tests.) 

2. **Round pipeline**

* **Active set (array)** → **vertex-parallel discharge with local buffers** → **parallel commit** (labels/excess/flags) → **parallel concat of discovered** → repeat; call **GlobalRelabel** per budget. (L1.1 structure.) 

3. **Conflict rule**

* Implement exactly (`d(v)<d(w)−1 || d(v)=d(w)+1 || (d(v)=d(w) && v<w)`), and **test admissibility using `d′(v)=d(w)+1`** (old neighbor labels) while relabels go to `d′`. (L1.1 37–45, 56–61; §3.1.)

4. **Atomics**

* Use **F&A** for neighbor `addedExcess` (**53**); **TAS** for first discovery (**54–55**, **65–66**); **CAS** in GR (**10–13**). Everything else is non-atomic within the round.

5. **Global relabeling policy**

* Budget with `β=12`, `freq=0.5`, `α=6`; trigger roughly each `~12n + 2m` scanned-edge units; run GR **between rounds** only. (L1.1 15–20; text footnote.)

6. **Edge scanning inside discharge**

* Treat as **serial per-vertex** despite the “parallel foreach” token in Listing—paper clarifies “edges are checked sequentially,” and the loop uses `break`. (Use contiguous adjacency for cache behavior.)

---

# “Porting” pointers for PBGL-style environments (distributed/edge-cut graphs)

> The paper’s code is shared-memory; PBGL is typically distributed (MPI). Here’s how to transpose safely:

* **Partitioning:** Own a vertex set per process; maintain **ghosts** for boundary neighbors’ `d`, `d′`, `e`, and `isDiscovered`. Use **synchronous supersteps** that mirror the paper’s rounds. (Round barriers are natural MPI steps.)

* **Within a process:** Keep the **same prsn kernel**: vertex-parallel discharge, local buffering, conflict rule (applies to local–local edges immediately).

* **Cross-partition edges:**

  * During discharge, pushes to remote neighbors become **messages** carrying `Δ` (excess increment) and an optional **discover** flag instead of `fetch-and-add / test-and-set`. Batch them; deliver at end-of-round.
  * At **commit**, apply received `Δ` to ghost or local `e`; **reduce** first-touch of remote discovery (minimize duplicates via per-round bitset and a per-dest rank compaction). This mimics the paper’s TAS semantics with message coalescing.

* **Labels (`d`/`d′`) consistency:**

  * Discharge reads **old `d(neighbor)`** (ghosts); relabel writes to local **`d′`**.
  * After commit, **exchange `d′→d` on ghosts** (one halo sync per round).

* **Global relabel:**

  * Implement **distributed reverse BFS** from sink using level-synchronous steps; **first-touch** ≈ CAS → use an **owner-decides** rule (owner rank atomicity) or MPI RMA with compare-and-swap if available; otherwise resolve by level + owner min. Trigger by the same budget.
  * Ensure GR is **not concurrent** with pushes—treat it as its own set of supersteps.
    (All mirrors paper intent.)

* **Winner rule across partitions:**

  * Needs only **old labels `d`** and vertex IDs; both are known (local or ghost). Decide **locally** on each endpoint; if both would push, allow only the endpoint that wins by rule; the other side sees “not win” and skips—no extra sync required (just consistent rule + same `d`). 

* **Heuristics to keep:**

  * FIFO-style active-set growth; per-rank prefix-sum concatenation + an all-to-all light exchange of newly remote-active vertices.
  * Periodic GR, not concurrent.

---

# Parameter crib sheet & constants

* **GR budget constants:** `β=12`, `freq=0.5`, `α=6` (Listing 1.1 comments and lines 17–20, 61). Roughly every `12n + 2m` scanned edges. 
* **Drop rule:** don’t keep `d(v)=n)` in working set (line 22, 75). 

---

# Quick “what to verify” when implementing

* **Only three atomics:** F&A (excess), TAS (discovery), CAS (GR discovery). If you find yourself adding more, you’re off-spec. (Lines **53–55**, **65–66**; GR **10–13**.)

* **Discharge scans edges sequentially** inside each vertex task; parallelism is at the **vertex** level per round. (Text §3, Listing semantics.) 

* **Relabels go to `d′`,** admissibility checks use **neighbor `d`**; commit `d′→d` only at end-of-round. (Lines **56–61**, **68–73**.) 

* **Global relabel never overlaps** with pushes; it’s invoked **between rounds** by the budget. (Lines **15–20**.) 

---

# Performance notes (so you know what to expect)

* On **rgg27/delaunay/nlpkkt240**, prsn beats hpf and is **~3× faster** than Hong–He at 32 threads; on **pld_spam**, prsn reaches **12×** over the best sequential with 40 threads. Hyper-threading didn’t help (bandwidth-bound).
* **Challenging:** europe.osm (road-like), rmf wide 4—parallel didn’t win there. 

---

# Handy citations to keep open

* **Algorithm narrative & why FIFO:** §2.1–3; FIFO vs highest-label observation.
* **Synchronous design & four-step sketch:** §3 (bulleted steps). 
* **Conflict rule + `d′` correctness:** §3.1. 
* **Pseudocode (prsn):** Listing 1.1. 
* **Pseudocode (GlobalRelabel):** Listing 1.2. 
* **Benchmarks + machine:** §4.2–4.3, figs/tables.

If you want, I can turn this into a one-page PDF “engineering crib sheet” you can print and annotate during your PBGL port.
