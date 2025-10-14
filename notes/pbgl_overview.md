# (AI-generated document)

---

# How PBGL is built (mental model)

PBGL is an extension of Boost.Graph that keeps (nearly) the same algorithm APIs but swaps in **distributed data structures** and a **BSP-style runtime** so your graph + property data live across MPI ranks. You still write SPMD code—every rank runs the same program—but PBGL gives you the pieces to keep global state consistent with minimal fuss. ([boost.org][1])

At a high level you combine:

* a **process group** (the communication substrate; MPI in practice),
* a **distributed graph** (most often the *distributed adjacency list*),
* **distributed property maps** (values that live “with” their owning vertex/edge + ghost caches), and
* sometimes **distributed queues** (for frontier-style traversals like BFS). ([boost.org][2])

Synchronization happens at explicit barriers (BSP “supersteps”), typically via `synchronize(pg)`. Between barriers you can also receive certain messages “early” via **triggers** or send **out-of-band (OOB)** requests that get immediate replies (handy for one-off lookups). ([boost.org][2])

---

# Core building blocks

## 1) Process group (MPI)

* Type: **`boost::graph::distributed::mpi_process_group`**
  Header: `<boost/graph/distributed/mpi_process_group.hpp>`
  It implements the PBGL “process group” interface on top of Boost.MPI and provides `send/receive`, `synchronize`, triggers, and OOB messaging. You can also tune batching parameters for message coalescing. ([boost.org][3])

**Programming model:** PBGL follows a relaxed **BSP** semantics:

* do local work
* send messages
* `synchronize(pg)` to enter the next superstep
  Messages sent in a superstep are available in the next one; with triggers you may process some messages “early.” ([boost.org][2])

## 2) Distributed graphs

### Distributed adjacency list

* Type template: `adjacency_list<OutEdgeListS, distributedS<ProcessGroup, VertexListS>, DirectedS, VertexProperty, EdgeProperty, GraphProperty, EdgeListS>`
* Header: `<boost/graph/distributed/adjacency_list.hpp>`
* Partitioning: **row-wise**—each rank owns a disjoint set of vertices and **all outgoing edges** of those vertices (for undirected graphs either endpoint may store the edge). ([boost.org][4])

**How to define one**

* Choose your local containers (e.g., `vecS`), choose a process group (MPI), and wrap `VertexListS` with `distributedS<...>`.
* Properties (bundled or otherwise) are stored only on the **owner** of the vertex/edge; access for nonlocal elements goes through distributed property maps (below). ([boost.org][4])

**Constructing graphs (three common ways)**

1. **Sequence constructors** from iterators that yield `(u,v)` (handy with PBGL’s generators like R-MAT).
2. **Adding by global vertex numbers** (you pass `n` total vertices and then add edges with `vertex(i,g)`).
3. **Named vertices** (opt in by specializing `internal_vertex_name` / `internal_vertex_constructor` so you can `add_edge("A","B", ...)`).
   All of these are documented with examples and caveats about who should add edges (avoid duplicates across ranks) and the final `synchronize`. ([boost.org][4])

**Concepts modeled**

* You get `Graph`, `IncidenceGraph`, etc., but note that *global* `VertexListGraph`/`EdgeListGraph` aren’t modeled because no single rank sees all vertices/edges. PBGL instead provides **Distributed Vertex/Edge List Graph** concepts (iterate locals). ([boost.org][4])

### Useful adaptors

* **`local_subgraph`**: a view that hides nonlocal items so you can run local computations cleanly. Header: `<boost/graph/distributed/local_subgraph.hpp>`. ([boost.org][5])
* **`vertex_list_adaptor`**: temporarily gives every process a global vertex list (via an all-gather) for algorithms that need `VertexListGraph` semantics (e.g., certain MST variants). Header: `<boost/graph/distributed/vertex_list_adaptor.hpp>`. Expect O(n) comms per node on build. ([boost.org][6])

### Input/Output & generators

* **METIS input**: `metis_reader` (edges/weights as iterators) + `metis_distribution` to reuse a precomputed partition. Header: `<boost/graph/metis.hpp>`. ([boost.org][7])
* **Synthetic graphs**: R-MAT, Erdős–Rényi, SSCA, Mesh, etc. (iterators you feed into the sequence constructors). R-MAT header: `<boost/graph/rmat_graph_generator.hpp>`. ([boost.org][1])

## 3) Distributed property maps

* The **workhorse** for algorithm state (colors, distances, ranks, …).
* Provide `get/put` for **nonlocal** keys by maintaining **ghost cells** and handling propagation during `synchronize`.
* You choose a **consistency model** (`cm_forward`, `cm_backward`, `cm_bidirectional`, plus `cm_flush`, `cm_reset`, `cm_clear`) and optionally a **reduction operator** to resolve concurrent updates and default values (e.g., min-distance, sum-accumulate). ([boost.org][8])

Typical patterns:

* **BFS/SSSP** use forward consistency; distance maps use a “min” reducer; visitors should mark roles: `set_property_map_role(vertex_distance, distance_map)`. ([boost.org][9])
* **PageRank** uses `cm_flush | cm_reset` and a reducer that **adds** incoming partial ranks each iteration. Header: `<boost/graph/distributed/page_rank.hpp>`. ([boost.org][8])

## 4) Distributed queue

* A **distributed adaptor** over a local queue for frontier-style algorithms; `push()` routes items to their owner rank, and global emptiness requires a synchronization semantics so the usual `while(!Q.empty())` pattern parallelizes correctly. You can enable/disable **polling** to trade ordering guarantees for fewer syncs. Header: see “Distributed queue adaptor” page. ([boost.org][10])

---

# Algorithms you get “out of the box”

PBGL keeps the sequential API shapes and adds **distributed versions** (plus a few specialized ones). Highlights:

* **BFS** – level-synchronized, with a distributed visitor; complexity is (O(V+E)) work in (d+1) supersteps (d = diameter). Requires a distributed queue & color map. ([boost.org][9])
* **SSSP / Dijkstra** – multiple backends: Eager, **Crauser et al.**, and **Delta-Stepping** (usually the best). Same visitor & named-parameter style as BGL; distance & predecessor maps are distributed. ([boost.org][11])
* **MST (Borůvka variants)**, **Connected/Strongly Connected Components**, **PageRank**, **Graph Coloring**, **Betweenness centrality**, **force-directed layout**, etc. (see the “Algorithms” list in the index). ([boost.org][1])

---

# Source layout (what to look at in the repo)

The GitHub repo is `boostorg/graph_parallel`. You’ll find:

* **`include/boost/graph/distributed/`** – headers for the process group, distributed adjacency list, adaptors, etc. (e.g., `mpi_process_group.hpp`, `adjacency_list.hpp`, `local_subgraph.hpp`, `vertex_list_adaptor.hpp`).
* **Algorithm headers** live under `boost/graph/...` (e.g., `breadth_first_search.hpp`, `dijkstra_shortest_paths.hpp`, and `distributed/page_rank.hpp` per the “Where Defined” sections).
* **`example/`** and **`test/`** contain runnable references for build + usage.
  Repo root: **boostorg/graph_parallel**. ([GitHub][12])

> Tip: the docs’ “Where Defined” blocks tell you the exact headers to include for each feature, even when the code lives in a different subfolder. For instance, BFS is documented under PBGL but included from `<boost/graph/breadth_first_search.hpp>`. ([boost.org][9])

---

# Practical “how do I use this?”

## Build & run

* **Requires** Boost.MPI + Boost.Serialization (for messaging) and the PBGL headers.
* Typical compile: use your MPI C++ compiler wrapper (`mpicxx`) and link Boost (MPI, serialization, etc.).
* Run with `mpirun -np <P> ./your_app`. (The PBGL docs consistently show MPI as the only process group backend.) ([boost.org][3])

## A tiny end-to-end starter (sketch)

```cpp
#include <boost/mpi/environment.hpp>
#include <boost/mpi/communicator.hpp>
#include <boost/graph/distributed/mpi_process_group.hpp>
#include <boost/graph/distributed/adjacency_list.hpp>
#include <boost/graph/breadth_first_search.hpp>
#include <boost/graph/iteration_macros.hpp>

struct VertexData {
  // bundle example (must be serializable if you add fields)
  template<class Ar> void serialize(Ar&, const unsigned int) {}
};

int main(int argc, char** argv) {
  boost::mpi::environment env(argc, argv);
  boost::mpi::communicator world;

  using PG   = boost::graph::distributed::mpi_process_group;
  using Graph = boost::adjacency_list<
      boost::vecS,
      boost::distributedS<PG, boost::vecS>,
      boost::directedS,
      VertexData>;

  Graph g(world);                   // empty distributed graph over this communicator
  const std::size_t n = 10;
  add_vertex(n, g);                 // collectively size to n vertices
  // Add a couple of edges (by global indices). Only one rank should add each edge.
  if (world.rank() == 0) {
    add_edge(vertex(0, g), vertex(1, g), g);
    add_edge(vertex(1, g), vertex(2, g), g);
  }
  synchronize(g.process_group());   // finalize construction

  // BFS from vertex 0; PBGL provides default distributed color map + queue
  breadth_first_search(g, vertex(0, g), boost::visitor(boost::bfs_visitor<>()));

  return 0;
}
```

This uses:

* `mpi_process_group` implicitly through the graph,
* distributed adjacency list with row-wise partitioning,
* distributed queue + color map inside BFS by default.
  See BFS and distributed adjacency list docs for parameterization and visitors with roles/reducers if you store distances, etc. ([boost.org][9])

---

# Things to watch out for (the “gotchas”)

* **Local vs global:** Many graph operations are only valid on **local** vertices/edges; if you need a global vertex list for an algorithm, build a **`vertex_list_adaptor`** (it does an all-gather; use judiciously). ([boost.org][4])
* **Ghosts & consistency:** A distributed property map’s `get()` on a remote key returns the **ghost** value, which may be stale until you `synchronize`. Pick the right **consistency flags** and **reduce** function, or explicitly `request()` before reading. ([boost.org][8])
* **Construction:** If multiple ranks insert the same edge, you’ll get duplicates. Designate a loader rank (often 0), or partition input by process. Then `synchronize()`. ([boost.org][4])
* **BFS semantics:** It’s **level-synchronized**; visitors can see `tree_edge` events even when another rank already discovered the target—design reducers and checks accordingly. ([boost.org][9])
* **Partitioning:** Default distributions are simple; for better cuts, use **`metis_distribution`** with your METIS partition files. ([boost.org][7])

---

# Handy pages (by task)

* **Architectural overview** (pictures + flow): how graphs, queues, property maps, and process group interact. ([boost.org][13])
* **Process group (BSP, triggers, OOB)**: what `synchronize` really means and how to do immediate replies. ([boost.org][2])
* **MPI process group reference**: exact header and knobs for batching. ([boost.org][3])
* **Distributed adjacency list**: definition, construction modes, named vertices, and concept notes. ([boost.org][4])
* **Distributed property map**: ghost cells, consistency, reductions, and the iterator/local map specializations. ([boost.org][8])
* **Distributed queue**: semantics for global emptiness and polling trade-offs. ([boost.org][10])
* **Algorithms index**: BFS, Dijkstra (Delta-Stepping, Crauser), MST, CC/SCC, PageRank, coloring, centrality, etc. ([boost.org][1])
* **Generators & METIS I/O**: R-MAT and METIS reader/distribution. ([boost.org][14])
* **PageRank**: flush/reset model and reducer example. ([boost.org][15])

If you want, I can turn this into a short “PBGL cheat sheet” PDF for your thesis appendix, or help you scaffold a minimal PBGL app (with CMake + MPI) that you can grow into your push-relabel work.

[1]: https://www.boost.org/doc/libs/1_89_0/libs/graph_parallel/doc/html/index.html "Parallel BGL Parallel Boost Graph Library"
[2]: https://www.boost.org/doc/libs/1_89_0/libs/graph_parallel/doc/html/process_group.html "Parallel BGL Parallel BGL Process Groups"
[3]: https://www.boost.org/doc/libs/1_89_0/libs/graph_parallel/doc/html/mpi_bsp_process_group.html "Parallel BGL MPI BSP Process Group"
[4]: https://www.boost.org/doc/libs/1_89_0/libs/graph_parallel/doc/html/distributed_adjacency_list.html "Parallel BGL Distributed Adjacency List"
[5]: https://www.boost.org/doc/libs/1_89_0/libs/graph_parallel/doc/html/local_subgraph.html "Parallel BGL Local Subgraph Adaptor"
[6]: https://www.boost.org/doc/libs/1_89_0/libs/graph_parallel/doc/html/vertex_list_adaptor.html "Parallel BGL Vertex List Graph Adaptor"
[7]: https://www.boost.org/doc/libs/1_89_0/libs/graph_parallel/doc/html/metis.html "Parallel BGL METIS Input Routines"
[8]: https://www.boost.org/doc/libs/1_89_0/libs/graph_parallel/doc/html/distributed_property_map.html "Parallel BGL Distributed Property Map"
[9]: https://www.boost.org/doc/libs/1_89_0/libs/graph_parallel/doc/html/breadth_first_search.html "Parallel BGL Breadth-First Search"
[10]: https://www.boost.org/doc/libs/1_89_0/libs/graph_parallel/doc/html/distributed_queue.html "Parallel BGL Distributed queue adaptor"
[11]: https://www.boost.org/doc/libs/1_89_0/libs/graph_parallel/doc/html/dijkstra_shortest_paths.html "Parallel BGL Dijkstra's Single-Source Shortest Paths"
[12]: https://github.com/boostorg/graph_parallel "GitHub - boostorg/graph_parallel: Boost.org graph_parallel module"
[13]: https://www.boost.org/doc/libs/1_89_0/libs/graph_parallel/doc/html/overview.html "An Overview of the Parallel Boost Graph Library"
[14]: https://www.boost.org/doc/libs/1_89_0/libs/graph_parallel/doc/html/rmat_generator.html "Parallel BGL R-MAT generator"
[15]: https://www.boost.org/doc/libs/1_89_0/libs/graph_parallel/doc/html/page_rank.html "Parallel BGL PageRank"
