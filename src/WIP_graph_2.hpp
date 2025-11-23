#ifndef GRAPH_HPP
#define GRAPH_HPP

#include <Kokkos_Core.hpp>

template <class DeviceType>
struct Graph {
    // -------------------------------------------------------
    // Types
    // -------------------------------------------------------
    using ExecutionSpace = typename DeviceType::execution_space;
    using MemorySpace    = typename DeviceType::memory_space;
    
    // Use signed integers to allow for sentinel values (-1) if necessary.
    // 'int' is generally faster than size_t on GPUs for register usage.
    using NodeIndex      = int; 
    using EdgeIndex      = int; 
    using ValueType      = long long; // Must support atomic_add

    // -------------------------------------------------------
    // Topology (CSR Format)
    // -------------------------------------------------------
    
    // CSR Offsets: Indices into entries. 
    // Size: num_nodes + 1
    // Access: Read-only random access.
    Kokkos::View<EdgeIndex*, DeviceType> row_map;

    // CSR Entries: The neighbor index for each edge. 
    // Size: num_edges
    // Access: Read-only, sequential access (coalesced).
    Kokkos::View<NodeIndex*, DeviceType> entries;

    // -------------------------------------------------------
    // Edge Properties (SoA Layout)
    // -------------------------------------------------------

    // Residual Capacity: c_f(u, v). 
    // Size: num_edges
    // Behavior: 
    // - Initialized to capacity c(u,v). 
    // - Updated NON-ATOMICALLY in 'Process' kernel (guaranteed by Win condition).
    // - Read frequently to check admissibility.
    Kokkos::View<ValueType*, DeviceType> residual_capacity;

    // Reverse Edge Index: Maps edge (u,v) -> index of (v,u). 
    // Size: num_edges
    // Behavior: Essential for updating the reverse arc in O(1) during a push.
    Kokkos::View<EdgeIndex*, DeviceType> reverse_edge;

    // -------------------------------------------------------
    // Vertex Properties (SoA Layout)
    // -------------------------------------------------------

    // Excess: e(v). Flow currently stored at node v.
    // Size: num_nodes
    // Behavior: 
    // - Read/Write by owner thread in 'Process' kernel. 
    // - Updated from 'added_excess' in 'Apply' kernel.
    Kokkos::View<ValueType*, DeviceType> excess;

    // Label: d(v). Distance estimate to sink. 
    // Size: num_nodes
    // Behavior: 
    // - Read-Only in 'Process' kernel (to check admissibility).
    // - Updated in 'Apply' kernel from 'new_label'.
    Kokkos::View<int*, DeviceType> label;

    // New Label: Buffer for label updates.
    // Size: num_nodes
    // Behavior:
    // - Written in 'Process' kernel if a node relabels.
    // - Read in 'Apply' kernel to update 'label'.
    // - Prevents race conditions on 'label' during the synchronous step.
    Kokkos::View<int*, DeviceType> new_label;

    // -------------------------------------------------------
    // Algorithm State / Work Management
    // -------------------------------------------------------

    // Active Set (Current): Nodes active in the current iteration.
    // Size: num_nodes (Capacity)
    // Behavior: Dense list of valid indices [0, num_active).
    Kokkos::View<NodeIndex*, DeviceType> current_active;

    // Active Set (Next): Nodes activated for the next iteration.
    // Size: num_nodes (Capacity)
    // Behavior: Populated via atomic_fetch_add on 'next_queue_size'.
    Kokkos::View<NodeIndex*, DeviceType> next_active;

    // Added Excess: Buffer for atomic updates.
    // Size: num_nodes
    // Behavior: 
    // - Pushes update this buffer atomically to avoid races on 'excess'.
    // - 'Apply' kernel reads this, adds to 'excess', and resets it to 0.
    Kokkos::View<ValueType*, DeviceType> added_excess;

    // Iteration Mask (Generational Marking).
    // Size: num_nodes
    // Behavior: 
    // - Stores the iteration number 'k' when the node was last added to a queue.
    // - To add node 'w' to the next queue, we atomic_exchange(mask[w], k+1).
    // - If the old value was != k+1, we successfully claimed the spot and add to queue.
    // - ELIMINATES the need to clear a flag array every iteration.
    Kokkos::View<int*, DeviceType> active_iteration_mask;

    // Queue sizes (managed as single-element Views for device access)
    Kokkos::View<size_t, DeviceType> current_queue_size;
    Kokkos::View<size_t, DeviceType> next_queue_size;

    // -------------------------------------------------------
    // Helpers
    // -------------------------------------------------------

    KOKKOS_INLINE_FUNCTION
    NodeIndex num_nodes() const { return excess.extent(0); }

    KOKKOS_INLINE_FUNCTION
    EdgeIndex num_edges() const { return entries.extent(0); }
};

#endif // GRAPH_HPP