#ifndef GRAPH_HPP
#define GRAPH_HPP

#include <Kokkos_Core.hpp>

// sections:
// [CORE TYPES]
// [GRAPH REPRESENTATION]
// [EDGE PROPERTIES]
// [VERTEX PROPERTIES]
// [STATE / WORK VIEWS]

template <class DeviceType>
struct Graph
{

    // [CORE TYPES]
    using ExecutionSpace = typename DeviceType::execution_space;
    using MemorySpace = typename DeviceType::memory_space;
    using NodeIndex = int;
    using EdgeIndex = int;
    using ValueType = long long; // Must support atomic_add

    using RowMapType = Kokkos::View<EdgeIndex *, DeviceType>;
    using EntriesType = Kokkos::View<NodeIndex *, DeviceType>;
    using ValueViewType = Kokkos::View<ValueType *, DeviceType>; // For capacity, excess
    using IndexViewType = Kokkos::View<EdgeIndex *, DeviceType>; // For reverse index
    using LabelViewType = Kokkos::View<int *, DeviceType>;
    using MaskViewType = Kokkos::View<int *, DeviceType>;

    // [GRAPH REPRESENTATION]

    // CSR Offsets: Indices into entries (edge list).
    // Size: num_nodes + 1
    // Read-only during algorithm
    Kokkos::View<EdgeIndex *, DeviceType> row_map;

    // CSR Entries: The neighbor index for each edge.
    // Size: num_edges
    // Access: Read-only, sequential access (coalesced).
    Kokkos::View<NodeIndex *, DeviceType> entries;

    // [EDGE PROPERTIES]

    // Residual Capacity: c_f(u, v).
    // Size: num_edges
    // Behavior:
    // - Initialized to capacity c(u,v).
    // - Updated in 'Process' kernel (NO ATOMICS YAY! Courtesy of Win condition).
    // - Read frequently to check admissibility.
    Kokkos::View<ValueType *, DeviceType> residual_capacity;

    // Reverse Edge Index: Maps edge (u,v) -> index of (v,u).
    // Size: num_edges
    // Access: Read-only during algorithm
    // - allows O(1) lookup for edges
    Kokkos::View<EdgeIndex *, DeviceType> reverse_edge;

    // [VERTEX PROPERTIES]

    // Excess: e(v). Flow currently stored at node v.
    // Size: num_nodes
    // Behavior:
    // - Read/Write by owner thread (local) in 'process' part of the algorithm.
    // - Updated from 'added_excess' in 'apply' part of the algorithm.
    Kokkos::View<ValueType *, DeviceType> excess;

    // Label: d(v). Distance estimate to sink.
    // Size: num_nodes
    // Behavior:
    // - Read-Only in 'process' part of the algorithm (to check admissibility).
    // - Updated (local) in 'apply' part of the algorithm from 'new_label'.
    Kokkos::View<int *, DeviceType> label;

    // New Label: Buffer for label updates.
    // Size: num_nodes
    // Behavior:
    // - Written in 'Process' kernel if a node relabels.
    // - Read (local) in 'apply' to update 'label'.
    // - Prevents race conditions on 'label' during the synchronous step.
    Kokkos::View<int *, DeviceType> new_label;

    // [STATE / WORK VIEWS]

    // Active Set (Current): Nodes active in the current iteration.
    // Size: <=num_nodes (Capacity)
    // Behavior: Dense list of valid indices [0, current_queue_size).
    Kokkos::View<NodeIndex *, DeviceType> current_active;

    // Active Set (Next): Nodes activated for the next iteration.
    // Size: <=num_nodes (Capacity)
    // Behavior: Populated via atomic_fetch_add on 'next_queue_size'.
    Kokkos::View<NodeIndex *, DeviceType> next_active;
    // no way around atomics, unless we use iteration count
    // to colour vertices for next step -- BUT that will mean
    // we will always launch full sized kernel (on all nodes)
    // out of which 99% of the time, 99% of vertices will
    // lay dormant -- massive memory bandwith for minimal work
    // + A lot of branch divergence in the "wavefronts" (or cuda counterpart)

    // Queue sizes (single-element Views for device access)
    Kokkos::View<size_t, DeviceType> current_queue_size;
    Kokkos::View<size_t, DeviceType> next_queue_size;

    // Added Excess: Buffer for atomic updates.
    // Size: num_nodes
    // Behavior:
    // - Pushes update this buffer atomically to avoid races on 'excess'.
    // need for atomics for when 2 nodes push into shared neighbour
    // - 'apply' reads (local) this, adds to 'excess', and resets it to 0.
    Kokkos::View<ValueType *, DeviceType> added_excess;

    // Iteration Mask. Used for deduplication of adding to active
    // Size: num_nodes
    // Behavior:
    // - Stores the iteration number 'k' when the node was last added to a queue.
    // - To add node 'w' to the next queue, we atomic_exchange(mask[w], k+1).
    // - If the old value was != k+1, we successfully claimed the spot and add to queue.
    // - ELIMINATES the need to clear a flag array every iteration.
    Kokkos::View<int *, DeviceType> active_iteration_mask;

    // NOTE: -- Work counter to count
    // work per vertex is not here,
    // because we can make 'process' parallel reduce
    // where each vertex returns work it has done
    // which can be summed into counter to check
    // for global relabel -- this saves memory
    // AND reset overhead we would need

    // Tracks the iteration a node was processed in
    Kokkos::View<int *, DeviceType> active_phase;

    // THIS IS FOR THE FUTURE EDGE-PARALLEL SHIFT

    // low/high queues
    // Kokkos::View<NodeIndex *, DeviceType> current_low;
    // Kokkos::View<NodeIndex *, DeviceType> next_low;
    // Kokkos::View<NodeIndex *, DeviceType> current_high;
    // Kokkos::View<NodeIndex *, DeviceType> next_high;

    // Kokkos::View<size_t, DeviceType> current_low_size;
    // Kokkos::View<size_t, DeviceType> next_low_size;
    // Kokkos::View<size_t, DeviceType> current_high_size;
    // Kokkos::View<size_t, DeviceType> next_high_size;

    // // GR
    // Kokkos::View<NodeIndex *, DeviceType> gr_current_active;
    // Kokkos::View<NodeIndex *, DeviceType> gr_next_active;

    // Kokkos::View<size_t, DeviceType> gr_current_size;
    // Kokkos::View<size_t, DeviceType> gr_next_size;

    // // arc
    // Kokkos::View<int *, DeviceType> current_arc;

    // -------------------------------------------------------
    // Helpers
    // -------------------------------------------------------

    KOKKOS_INLINE_FUNCTION
    NodeIndex num_nodes() const { return excess.extent(0); }

    KOKKOS_INLINE_FUNCTION
    EdgeIndex num_edges() const { return entries.extent(0); }
};

#endif // GRAPH_HPP