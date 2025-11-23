Below is the **Micro-Operation Walkthrough** of the "prsn" algorithm (Listing 1.1 in the PDF) mapped explicitly to the `Graph` structure.

We assume we are in **Iteration `k`**.
* **`current_active`**: Contains nodes active for this iteration.
* **`next_active`**: Empty. `next_queue_size` is 0.
* **`active_iteration_mask`**: Stores the last iteration `k` a node was added to a queue.

---

### Kernel 1: Process (Push & Relabel)
**Goal:** Discharge excess flow from active nodes and calculate new labels if stuck.
**Parallel Policy:** `Kokkos::parallel_for` over `current_active` range `[0, current_queue_size)`.

#### 1. Thread Initialization (Register Load)
* **Op:** Thread `t` grabs index `i`.
* **Read:** `u = current_active(i)`
* **Read:** `e_u = excess(u)` (Load current flow into register).
* **Read:** `d_u = label(u)` (Load current height into register).
* **Init:** `min_d = INFINITY` (Register to track lowest neighbor for potential relabel).
    * *Optimization:* We also initialize `new_label(u) = d_u` here, or handle it conditionally later. Let's assume we handle it later.

#### 2. The Edge Scan Loop (Memory Intensive)
* **Op:** Loop `j` from `row_map(u)` to `row_map(u+1)`.
* **Read:** `v = entries(j)`
    * *Hardware Note:* `entries` is accessed sequentially. Perfect memory coalescing.
* **Read:** `uv_cap = residual_capacity(j)`
    * *Hardware Note:* `residual_capacity` aligned with `entries`. Perfect coalescing.
* **Check:** `if (uv_cap == 0) continue;` (Skip saturated edges).
* **Read:** `d_v = label(v)`
    * *Hardware Note:* Random access (Scatter/Gather). This is the main latency cost.
* **Logic (Track Min Height):** `min_d = min(min_d, d_v + 1);`
* **Logic (Admissibility):** `bool admissible = (d_u == d_v + 1);`

#### 3. The "Push" Operation (Conditional)
* **Check:** `if (admissible && e_u > 0)`
    * **Logic (Win Condition):** `if (Win(u, v))`
        * *Note:* `Win(u,v)` compares `d_u, d_v` and `u, v` indices. Purely arithmetic, no memory access.
        * **If Win:** We have exclusive right to modify this edge.
            * `delta = min(e_u, uv_cap)`
            * **Write (Edge):** `residual_capacity(j) -= delta`
                * *Non-Atomic:* Safe because of Win condition.
            * **Read (Reverse Map):** `rev_idx = reverse_edge(j)`
            * **Write (Reverse Edge):** `residual_capacity(rev_idx) += delta`
                * *Non-Atomic:* Safe because of Win condition.
            * **Write (Self Excess):** `e_u -= delta` (Register update only).
            * **Atomic Write (Neighbor Excess):** `Kokkos::atomic_add(&added_excess(v), delta)`
                * *Atomic:* Necessary because other nodes might push to `v` at the same time.
            * **Queue Neighbor (`v`):**
                * `old_k = atomic_exchange(&active_iteration_mask(v), k + 1)`
                * `if (old_k != k + 1)`:
                    * `idx = atomic_fetch_add(&next_queue_size, 1)`
                    * `next_active(idx) = v`

#### 4. The "Relabel" Operation (Post-Loop)
* **Check:** `if (e_u > 0)` (Still have excess after trying all edges?)
    * **Write:** `new_label(u) = min_d`
        * *Note:* We write to `new_label`, NOT `label`. If we wrote to `label(u)`, we would corrupt the `d_v + 1` check for neighbors currently processing `u`.
    * **Queue Self (`u`):**
        * Since `u` still has excess, it must be active next round.
        * `old_k = atomic_exchange(&active_iteration_mask(u), k + 1)`
        * `if (old_k != k + 1)`:
            * `idx = atomic_fetch_add(&next_queue_size, 1)`
            * `next_active(idx) = u`
* **Write Back (Self Excess):**
    * *Refined Logic:* We should write the final register `e_u` back to global `excess(u)` *at the end of this thread*.
    * **Write:** `excess(u) = e_u`

---

### Kernel 2: Apply (State Update)
**Goal:** Apply the buffered flow and label changes to global state.
**Parallel Policy:** `Kokkos::parallel_for` over `next_active` range `[0, next_queue_size)`.

* **Op:** Thread `t` grabs index `i`.
* **Read:** `u = next_active(i)`
* **Read/Write (Apply Flow):**
    * `delta = added_excess(u)`
    * `if (delta > 0)`:
        * `excess(u) += delta`
        * `added_excess(u) = 0` (Reset for next iteration).
* **Read/Write (Apply Label):**
    * `d_next = new_label(u)`
    * `d_current = label(u)`
    * `if (d_next > d_current)`:
        * `label(u) = d_next`
        * `new_label(u) = 0` (Optional reset, or just overwrite next time).

---

### Final Analysis of Graph Properties

Based on this detailed walk, here is the exact disposition of every property:

1.  **`residual_capacity`**: **Read/Write**. Needs perfect SoA layout with `entries` for the scan loop.
2.  **`reverse_edge`**: **Read Only**. Used to find the reverse index `rev_idx`. Crucial for performance.
3.  **`excess`**: **Read/Write**. Read once at start of Kernel 1, written once at end of Kernel 1 (remaining), written once in Kernel 2 (incoming).
4.  **`added_excess`**: **Atomic Write / Read-Reset**. The buffer for inter-node communication. **Essential.**
5.  **`label`**: **Read Only (Kernel 1) / Write (Kernel 2)**. Used heavily for admissibility checks.
6.  **`new_label`**: **Write (Kernel 1) / Read (Kernel 2)**. **We MUST add this to the struct.** The previous walkthrough missed that we cannot update `label` in-place without race conditions.
7.  **`active_iteration_mask`**: **Atomic Exchange**. The "Coloring" optimization we discussed.
8.  **`next_active` / `current_active`**: Double buffered queues.
