# Assignment of Master's Thesis

| Field | Details |
| :--- | :--- |
| **Title** | **Portable High-Performance Implementation of the Maximum Flow Problem using Kokkos** |
| **Student** | Bc. Radek Cichra |
| **Supervisor** | doc. RNDr. Dušan Knop, Ph.D. |
| **Study Program** | Informatics |
| **Branch / Specialization** | System Programming |
| **Department** | Department of Theoretical Computer Science |
| **Validity** | Until the end of summer semester 2026/2027 |

## Instructions

Graph algorithms are fundamental to solving complex problems in diverse domains, yet their practical implementation on modern heterogeneous hardware presents significant challenges due to irregular memory access patterns and data-dependent control flow.

The goal of this thesis is to investigate the feasibility and performance characteristics of implementing such irregular algorithms using modern **Performance Portability frameworks**. The work will focus specifically on the **Maximum Flow problem** and the **Push-Relabel algorithm family**. Unlike traditional approaches that require maintaining separate codebases for CPUs and GPUs, this thesis aims to utilize the **Kokkos C++ library** to create a single, device-agnostic implementation.

The student will implement a synchronous, bulk-parallel variant of the Push-Relabel algorithm (PRSN), tailored to map efficiently to the throughput-oriented architecture of GPUs while remaining executable on multi-core CPUs. The work will conclude with a comparative study analyzing the trade-offs between high-level abstraction and raw hardware performance.

## Objectives

1.  **Theoretical Analysis:** Review state-of-the-art parallel approaches for the Maximum Flow problem, with a specific focus on synchronous bulk-parallel variants suitable for GPU execution (e.g., *Baumstark et al., 2015*).
2.  **Implementation:** Implement the Synchronous Parallel Push-Relabel (PRSN) algorithm using the **Kokkos ecosystem**. The implementation must be designed to compile for multiple backends (e.g., CPU via OpenMP/Threads, GPU via CUDA/HIP) from a single source codebase.
3.  **Performance Evaluation:** Benchmark the implementation on a representative set of large-scale graphs (e.g., real-world sparse matrices, small-world networks) to evaluate scalability and throughput.
4.  **Comparative Study (optional):** Analyze the "abstraction penalty" (portability tax) and performance portability by comparing execution times and memory bandwidth utilization across different hardware architectures and, where feasible, against available reference implementations.