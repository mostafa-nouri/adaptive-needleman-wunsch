# Parallel Needleman-Wunsch HPC Benchmarking Suite

This repository contains the official production-grade benchmarking suite for high-performance parallel sequence alignment. It implements our novel **PALS Engine** alongside comprehensive implementations of six pioneering parallel paradigms from the literature.

## 🔬 Evaluated Frameworks and Baselines

This suite unifies the following parallel architectures under identical microarchitectural and hardware constraints:

*   **PALS Engine (This Work):** An adaptive, linear-space $O(N)$ engine leveraging a closed-loop cumulative hysteresis window for dynamic row-chunk resizing and atomic straggler eviction.
*   **Static Row Polling [2]:** A multi-threaded, row-wise matrix partitioning layout that uses atomic synchronization cells to check upward cell dependencies.
*   **2D Block Wavefront [7]:** An efficient block-based parallel framework that tiles the dynamic programming matrix into fixed square grids handled via POSIX conditional flags.
*   **Static Staggered Barrier [3]:** A staggered wavefront row-band approach bounded by global hardware synchronization barriers at the end of each segment.
*   **Anti-Diagonal Barrier [8]:** A vectorized parallel model mapping independent diagonal wavefront cells across concurrent processing cores.
*   **Task Pool Grid [9]:** A distributed master-worker many-core task queue layout designed for fine-grained chunk handling.
*   **Parallel Prefix Scan [4]:** A parallel prefix scan approach utilizing associative prefix operators to resolve column-wise data dependencies in logarithmic time steps.

## 📂 Repository Directory Guide
* `/src/baselines/`: Contains independent, optimized C++ source implementations for all six literature benchmarks.
* `/src/common/`: Hosts the unified nucleotide matching matrix logic used to guarantee exact scoring correctness across all variants.
* `/benchmark/hpc_experiment_harness.py`: An automated testing script that simulates hardware throttling profiles and logs performance metrics directly to a master spreadsheet (`scientific_report.csv`).

## 🛠️ Automated Evaluation Harness

To guarantee absolute experimental fairness, you can run all 7 algorithms consecutively across Phase A, Phase B, and Phase C hardware conditions using the automated script:

```bash
# Compile all source engines via the unified bash automation
cd src/ && make all

# Run the master evaluation sweep (Requires root privileges to set CPU governors)
sudo python3 benchmark/hpc_experiment_harness.py
```

## 📊 Scientific Data Graphing
Plotting utilities are fully localized with the academic `B Nazanin` font to output publication-ready vector charts:
* `plot_benchmarks.py`: Compiles raw wall-clock runtime averages.
* `plot_speedup.py`: Renders the empirical Amdahl parallel speedup factor.
* `plot_scalability.py`: Maps the $O(N)$ linear space scaling trajectories up to 62.5 Billion cells.
