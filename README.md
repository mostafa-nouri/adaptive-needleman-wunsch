# Parallel Needleman-Wunsch HPC Benchmarking Suite

This repository contains the official, publication-grade high-performance computing (HPC) benchmarking suite for parallel Needleman-Wunsch global sequence alignment. It unifies our proposed **Cumulative Hysteresis Adaptive Scheduling Engine (PALS)** alongside comprehensive implementations of alternative parallel paradigms and classic baselines under identical microarchitectural and hardware constraints.

## 🔬 Core Repository File Inventory

The suite consists of 12 dedicated C++ implementations structured to evaluate memory-space and runtime scalability:

### 1. Proposed Framework (Our Method)
*   `nw_adaptive_linear_space.cpp`: Flagship engine of this research. Implements the closed-loop cumulative hysteresis window for dynamic row-chunk resizing and atomic straggler eviction within strict linear space $O(N)$ boundaries.
*   `nw_adaptive_hysteresis.cpp`: Implementation of our adaptive hysteresis scheduling pipeline utilizing a full quadratic matrix layout $O(MN)$ layout for comparative overhead tracking.

### 2. Classic Sequential Baselines (Single-Core)
*   `nw_classic_sequential_linear_space.cpp`: Optimized single-threaded reference baseline operating within linear space boundaries, used to calculate empirical parallel speedup factors for huge datasets.
*   `nw_classic_quadratic.cpp`: Standard single-threaded textbook Needleman-Wunsch implementation keeping the full matrix allocated in memory.

### 3. Parallel Baselines from Literature
*   `nw_static_row_polling.cpp` [Ref 2]: A row-wise matrix partitioning layout using atomic synchronization cells to track upward dependency constraints.
*   `nw_block_wavefront_2d.cpp` [Ref 7]: Tiling the dynamic programming matrix into fixed square grids synchronized via POSIX conditional flags.
*   `nw_static_staggered_barrier.cpp` [Ref 3]: A staggered wavefront row-band approach bounded by global hardware synchronization barriers.
*   `nw_diagonal_barrier.cpp` [Ref 8]: Vectorized parallel execution mapping independent diagonal anti-wavefront cells across processing cores.
*   `nw_task_pool_grid.cpp` [Ref 9]: A distributed master-worker many-core task queue layout for fine-grained chunk handling.
*   `nw_parallel_prefix.cpp` [Ref 4]: A log-time parallel prefix scan model to resolve column-wise data dependencies.

### 4. Legacy and Evolution Records
*   `nw_parallel_prefix_original.cpp`: Unmodified structural prefix formulation used during early evaluation stages.
*   `nw_static_staggered_barrier-old.cpp`: Legacy staggered layout utilizing fixed hard-coded thread configurations.

## ⚙️ Automated Testing and Execution Harnesses

The `benchmark/` subdirectory contains Python automation engines to systematically recreate hardware throttling profiles and scale evaluation datasets:

*   `generate_large_benchmarks.py`: Synthetic dataset generator. Generates 10 independent pairs of sequences scaling incrementally from 50,000 to 250,000 nucleotides (`seq1_50.fasta` up to `seq2_250.fasta`) to stress-test asymptotic boundaries.
*   `hpc_experiment_harness.py`: Core hardware stress harness. Loops through all parallel variants under dynamic frequency throttling (Phase B) and explicit microarchitectural core stall injections (Phase C). Logs wall-clock latencies directly to `scientific_evaluation_report.csv`.
*   `hpc_multi_dataset_runner.py`: Asymptotic scalability sweeper. Consecutively executes 3-trial sweeps for both the classic baseline and our PALS parallel engine across all 10 generated datasets, dumping results to dedicated CSV spreadsheets.

## 📊 Localized Charts and Figures

Plotting utilities are fully localized with the academic `B Nazanin` Persian font configuration and explicit slash separator (`/`) formatting masks to output publication-ready vector charts:
*   `plot_benchmarks.py`: Compiles raw wall-clock running time averages across Phase A, B, and C hardware conditions.
*   `plot_speedup_benchmarks.py`: Renders the empirical Amdahl parallel speedup factors relative to the sequential line.
*   `plot_scalability_curve.py`: Maps the linear $O(N)$ vs. quadratic $O(MN)$ asymptotic scaling trajectories up to 62.5 Billion cells.

## 🛠️ Step-by-Step Benchmarking Instructions

To execute the entire software workflow cleanly out of the box, run the following pipeline in your Linux terminal:

```bash
# 1. Navigate to the automation subdirectory
cd benchmark/

# 2. Generate all 10 scaling fasta dataset pairs
python3 generate_large_benchmarks.py

# 3. Execute the hardware resilience sweeps (Requires sudo to set CPU governors)
sudo python3 hpc_experiment_harness.py

# 4. Run the multi-dataset scalability profiling harness
python3 hpc_multi_dataset_runner.py

# 5. Compile all print-ready Persian vector plots
python3 plot_benchmarks.py
python3 plot_speedup_benchmarks.py
python3 plot_scalability_curve.py
```

## 📜 Academic License
This software framework is distributed under the terms of the open-source permissive **MIT License**.
