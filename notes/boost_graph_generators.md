# (AI-generated document)

---

# BGL (single-process)

## Erdős–Rényi — G(n,p) (and G(n,m) via helpers)

**What it tends to create**

* Random graphs with **binomial/Poisson-like degree distribution** (for modest p).
* **Low clustering**, **small average path length** once above connectivity threshold.
* Can be **directed or undirected**; can forbid self-loops; multiedges possible with the unsorted iterator.

**Suitable for**

* Baseline/neutral randomness; quick **stress tests** free of structure.
* Studying **threshold phenomena** (connectivity, giant component).
* Control experiments vs structure-heavy models (compare against small-world / scale-free).

**Knobs**

* `n` (vertices), `p` (edge prob) — or approximate `m` via `add_random_edges`.
* `allow_self_loops` (usually false).
* Pick **sorted** iterator for simple graphs; **unsorted** if you’re okay with multiedges.

---

## Watts–Strogatz Small-World

**What it tends to create**

* **High clustering** like lattices + **short path lengths** like random graphs.
* Degree ≈ `k` (regular-ish), then perturbed by rewiring.

**Suitable for**

* **Routing / BFS** benchmarks where local clusters matter.
* **Diffusion/epidemic** processes that mix local & long-range contacts.
* Testing algorithms sensitive to **clustering** but not heavy power-law hubs.

**Knobs**

* `n` (vertices), `k` (initial ring degree, even), `beta` (rewiring prob).
* `beta≈0` → lattice; `beta≈0.1` → classic small-world; `beta→1` → ER-like.

---

## PLOD (Power-Law Out-Degree) — scale-free

**What it tends to create**

* **Heavy-tailed degree distribution** (few hubs, many low-degree nodes).
* Low average path length; clustering depends on parameters (often modest).

**Suitable for**

* **Robustness/attack** studies (remove hubs and watch fragmentation).
* Algorithms impacted by **hubs** (priority queues, max-flow heuristics, label-prop).
* Modeling **social/web**-like graphs where degree varies by orders of magnitude.

**Knobs**

* `n` (vertices), exponent/shape (often `alpha`), and a target mean out-degree.
* Usually **directed**; can project to undirected if needed.

---

## Grid Graphs (n-D lattice)

**What it tends to create**

* Perfect **lattices** in 1D/2D/… with strong geometric locality.
* **High diameter**, **very low clustering** (except with diagonals you add yourself).
* Degree bounded (2D 4-neighbors unless you extend).

**Suitable for**

* **Shortest path** / **max-flow** tests with **planar/mesh** structure.
* **Image/terrain** algorithms, PDE discretizations, pathfinding (A*, Dijkstra) realism.

**Knobs**

* Dimension `D` and per-axis sizes; optional wrapping/diagonals are on you.
* Great for reproducible instances without RNG.

---

## Random Spanning Tree (of your graph)

**What it tends to create**

* A **uniform random spanning tree** sampled from the given graph (Wilson’s algorithm).
* Tree inherits your graph’s vertex set; edges form an acyclic backbone.

**Suitable for**

* **Baseline trees** for cut/flow comparisons; **initializations** for heuristics.
* Studying **effective resistance**/Kirchhoff indices, or sampling diverse trees.

**Knobs**

* Provide RNG, optional root, and property maps; works on directed/undirected (interprets appropriately).

---

## Random Utilities (sprinkle/construct)

**What they create**

* Simple ways to add **m random edges**, sample **random vertices/edges**, or build quick random graphs without a formal model.

**Suitable for**

* Quick-and-dirty **perturbations** of an existing graph.
* Building G(n,m)-style graphs when you want **exact edge count**.

**Knobs**

* `m` (how many edges), RNG, and your base graph container.

---

# PBGL (distributed / MPI)

## R-MAT (Graph500-style scale-free)

**What it tends to create**

* **Power-law** degree, **community structure**, **self-similar** adjacency (quadrant bias).
* Very popular for **parallel BFS/SSSP** and Graph500 benchmarks.

**Suitable for**

* **Scalability** testing across ranks; **frontier-heavy** traversals.
* Stressing load balance, communication, and ghost-vertex handling.

**Knobs**

* Number of vertices/edges (often via scale and edgefactor).
* Quadrant probabilities `(a,b,c,d)` tune community vs randomness.
* Construct via **edge-iterator pairs on every rank** with the same seed/params.

---

## PBGL with Erdős–Rényi (sorted iterator)

**What it tends to create**

* Same distributional behavior as single-process ER, but built in parallel.

**Suitable for**

* **Distributed baselines** for traversals/flows without structure bias.

**Knobs**

* Same as ER: `n`, `p`, seed — ensure **identical iterator sequence** on all ranks.

---

# Picking the right generator (quick heuristics)

* Need **neutral randomness** to sanity-check correctness? → **Erdős–Rényi**.
* Need **short paths + high clustering** (social-ish, but no big hubs)? → **Small-World**.
* Need **hub-dominated** structure, robustness/attack studies, web-like graphs? → **PLOD** (or **R-MAT** for distributed).
* Need **geometric locality / planarity** for pathfinding or flow on grids? → **Grid**.
* Need a **tree baseline** over an existing graph? → **Random Spanning Tree**.
* Need exact edge count or to **perturb** something you already built? → **Random utilities** (G(n,m)/sprinkle).

---