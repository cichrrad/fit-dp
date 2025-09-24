# State of Boost for graphs (BGL & PBGL)

* **BGL = mature, header-only graph library** with generic concepts, graph types (`adjacency_list`, etc.), and a broad algorithm set. ([Boost][1])
* **PBGL = distributed/parallel extension** exposing distributed graphs + algorithms with BGL-like APIs; strong for BFS/SSSP/MST/CC/PageRank/Coloring, etc. ([Boost][2])

## What’s implemented (high-level)

* **Core searches & structure:** BFS/DFS; topological sort; components (connected/strongly/biconnected); planarity tools; orderings (Cuthill–McKee, etc.). ([Boost][1])
* **Shortest paths:** Dijkstra, Bellman–Ford, Johnson, Floyd–Warshall, DAG shortest paths, A\*. ([Boost][1])
* **Spanning trees:** Kruskal, Prim (+ utilities). ([Boost][1])
* **Cuts/centrality/layout:** Stoer–Wagner min-cut; betweenness/closeness/degree centrality; multiple layouts. ([Boost][3])
* **Network flow (sequential, in BGL):**

  * **Max-flow:** Edmonds–Karp; Push–Relabel; Boykov–Kolmogorov. ([Boost][4])
  * **Min-cost max-flow:** Successive shortest path (non-negative weights); Cycle-canceling; plus `find_flow_cost`. ([Boost][5])
* **PBGL distributed algorithms:** BFS; Dijkstra variants (eager, Crauser, delta-stepping); DFS; MST (Borůvka family); CC & SCC; PageRank; graph coloring; force-directed layout; s-t connectivity; betweenness centrality. ([Boost][2])

## Code presence (repos)

* **BGL headers in tree (develop/master):** algorithm headers under `include/boost/graph` (e.g., `push_relabel_max_flow.hpp`, `edmonds_karp_max_flow.hpp`, `stoer_wagner_min_cut.hpp`, min-cost flow headers). ([GitHub][6])
* **PBGL headers:** distributed algorithms under `graph_parallel/include/boost/graph/distributed`. ([GitHub][7])

## Notable gaps / under-represented areas

* **No distributed max-flow / min-cost flow in PBGL.** The PBGL algorithm list omits flow; existing flow is only in sequential BGL. (Opportunity: parallel push-relabel, Dinic, cost-scaling.) ([Boost][2])
* **Dinic’s algorithm** (sequential) is **not listed** among BGL flow routines—adding a high-quality Dinic with layered networks & blocking flows could fill a common baseline. (Confirm via BGL TOC & flow docs.) ([Boost][8])
* **Min-cost flow features:** limited support for **lower/upper bounds, node demands/supplies, negative costs via potentials,** capacity/cost-scaling variants, and **circulation** formulations. Extending current SSP/cycle-canceling APIs with named parameters & invariants would be valuable. ([Boost][5])
* **Cut utilities:** richer **min s-t cut extraction**, **Gomory–Hu tree** (all-pairs min-cut) helpers are not front-and-center—room for a consistent API layer around existing max-flow/min-cut. (BGL exposes Stoer–Wagner; s-t min-cut is implicit via max-flow.) ([Boost][3])
* **Docs & examples:** PBGL lacks narrative/examples for flow-like workloads; adding **DIMACS I/O** style examples and perf notes (partitioning, ghost vertices, comms) would help adoption. ([Boost][2])

## Links (docs & repos used)

* **BGL docs (index/TOC):** BGL overview & algorithms; table of contents lists min-cost flow pages. ([Boost][1])
* **PBGL docs (index):** Distributed algorithm list (no flow). ([Boost][2])
* **Specific BGL algorithm docs:**

  * Push–Relabel max-flow, Edmonds–Karp max-flow, SSP min-cost, Cycle-canceling, Stoer–Wagner min-cut. ([Boost][9])
* **GitHub trees:**

  * BGL (develop) and (master) `include/boost/graph/` roots; PBGL distributed headers. ([GitHub][6])

[1]: https://www.boost.org/doc/libs/latest/libs/graph/doc/index.html "The Boost Graph Library"
[2]: https://www.boost.org/doc/libs/latest/libs/graph_parallel/doc/html/index.html "Parallel BGL Parallel Boost Graph Library"
[3]: https://www.boost.org/doc/libs/1_83_0/libs/graph/doc/stoer_wagner_min_cut.html?utm_source=chatgpt.com "Stoer–Wagner Min-Cut - Boost Graph Library"
[4]: https://www.boost.org/libs/graph/doc/edmonds_karp_max_flow.html?utm_source=chatgpt.com "Boost Graph Library: Edmonds-Karp Maximum Flow"
[5]: https://www.boost.org/libs/graph/doc/successive_shortest_path_nonnegative_weights.html?utm_source=chatgpt.com "Successive Shortest Path for Min Cost Max Flow"
[6]: https://github.com/boostorg/graph/tree/develop/include/boost/graph "graph/include/boost/graph at develop · boostorg/graph · GitHub"
[7]: https://github.com/boostorg/graph_parallel/tree/develop/include/boost/graph/distributed "graph_parallel/include/boost/graph/distributed at develop · boostorg/graph_parallel · GitHub"
[8]: https://www.boost.org/libs/graph/doc/table_of_contents.html?utm_source=chatgpt.com "Table of Contents: Boost Graph Library"
[9]: https://www.boost.org/libs/graph/doc/push_relabel_max_flow.html?utm_source=chatgpt.com "Push-Relabel Maximum Flow - Boost Graph Library"
