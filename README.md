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

---

# Repo structure

> This branch has basically the same structure as the main one, but the source code files are for the alternative approach, described in thesis **Chapter 4, Section 7**

```
.
├── .devcontainer/              # Environment for dev + running, assumes CUDA GPU 
├── helpers/                    # Ruby pre-processing scripts + convertors...
├── input/                      # Input data (not present due to size) + mock
├── profile_tools/              # .so profiling hooks
├── reference_implementations/  # source codes for all reference implementations
├── src/                        # KNFS multipar source files
├── CMakeLists.txt              
├── CMakePresets.json           
├── Gemfile                     # 'package.json', but for Ruby
├── Gemfile.lock                # 'package-lock.json', but for Ruby
├── Makefile                    # (look here for neat build macros)
├── README.md                   
└── main.cpp                    
```
# How to use

This branch usage is very analogous to the main one. Quick and dirty demo can once again be done with

```
make generate_graph ; make to_dimacs; make run_cpu_multipar # or run_gpu_multipar
```

# Notes

* This approach was not as battle tested as the main one, but it did seem to pass the whole benchmark suite (at least the GPU variant), modulo the instability mentioned in the thesis.

* CPU compiled version should work too, but Kokkos fetches max pseudo warp size as 1024 and it can crash. If you lower this based on your CPU (I am assuming <= logical core count works), it should work. To do so, modify `pseudo_warp_size` variable in `main.cpp`, ~ line 52.