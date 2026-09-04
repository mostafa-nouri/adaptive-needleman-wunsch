import subprocess
import re
import math
import csv
import os
from datetime import datetime

OUTPUT_CSV = "scientific_evaluation_report.csv"
REPETITIONS = 3  # 1 for validation, increase to 3 or 5 for publication

# --- PASSED DATASET PATH CONFIGURATION KNOB ---
# Maps cleanly out of the benchmark/ folder into the master dataset/ catalog directory
SEQ1_PATH = "../dataset/delta1.fasta"
SEQ2_PATH = "../dataset/omicron1.fasta"

# =========================================================================
# --- AUTOMATED FASTA LENGTH DETECTOR ENGINE ---
# =========================================================================
def get_fasta_sequence_length(fasta_path):
    """Parses a FASTA file and returns the exact character count of the strand."""
    if not os.path.exists(fasta_path):
        print(f"❌ CRITICAL ERROR: Fasta file not found at '{fasta_path}'")
        exit(1)
        
    length = 0
    with open(fasta_path, 'r') as file:
        for line in file:
            # Skip the FASTA descriptor metadata header line
            if line.startswith('>'):
                continue
            # Accumulate clean alphabetical letter counts only
            length += len(line.strip())
    return length

# Dynamically calculate sequence metrics on the fly before running binaries
LEN1 = get_fasta_sequence_length(SEQ1_PATH)
LEN2 = get_fasta_sequence_length(SEQ2_PATH)
TOTAL_CELLS = LEN1 * LEN2
# =========================================================================

# FIX: Mapped out accurately into the unified src/ folder structure hierarchies
SOURCES = {
    "OUR_ADAPTIVE_HYSTERESIS": "../src/pals_engine/nw_adaptive_hysteresis.cpp",
    "PAPER1_STATIC_ROW_POLLING": "../src/baselines/nw_static_row_polling.cpp",
    "PAPER2_BLOCK_WAVEFRONT_2D": "../src/baselines/nw_block_wavefront_2d.cpp",
    "PAPER3_STATIC_STAGGERED_BARRIER": "../src/baselines/nw_static_staggered_barrier.cpp",
    "PAPER4_DIAGONAL_BARRIER": "../src/baselines/nw_diagonal_barrier.cpp",
    "PAPER5_TASK_POOL_GRID": "../src/baselines/nw_task_pool_grid.cpp",
    "PAPER6_PARALLEL_PREFIX": "../src/baselines/nw_parallel_prefix.cpp"
}

SPIN_PHASE_B = 5
SPIN_PHASE_C = 20

CONDITIONS = {
    "A_PRISTINE_UNIFORM": {
        "flags": [], 
        "governor": "performance" 
    },
    "B_ASYMMETRIC_THROTTLED": {
        "flags": ["-DSIMULATE_DELAY", f"-DSPIN_COUNT={SPIN_PHASE_B}"], 
        "governor": "powersave" 
    },
    "C_EXTREME_STALL": {
        "flags": ["-DSIMULATE_DELAY", f"-DSPIN_COUNT={SPIN_PHASE_C}"], 
        "governor": "powersave"
    }
}

def set_system_hardware_governor(governor_mode):
    try:
        print(f"       [System OS] Setting cpufreq governors live to: {governor_mode}...")
        subprocess.run(["sudo", "cpupower", "frequency-set", "-g", governor_mode], stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=True)
    except Exception:
        print("       [⚠️ System Warning] Could not modify bare-metal governor. Proceeding with software simulation loops.")

def compile_binary(source_file, binary_name, flags_list):
    compile_cmd = ["g++", "-O3", "-lpthread"]
    if flags_list:
        compile_cmd.extend(flags_list)
    compile_cmd.extend([source_file, "-o", binary_name])
    subprocess.run(compile_cmd, check=True)

def calculate_metrics(output_text, wall_time):
    score_match = re.search(r"ALIGNMENT_SCORE:\s*(-?\d+)", output_text)
    cores_match = re.search(r"CORES_USED:\s*(\d+)", output_text)
    
    score = int(score_match.group(1)) if score_match else 0
    cores = int(cores_match.group(1)) if cores_match else 1
    
    workloads = []
    matches = re.findall(r"THREAD_WORKLOAD:\s*Thread\s*\d+\s*processed\s*(\d+)\s*rows", output_text)
    for m in matches:
        workloads.append(int(m))
        
    if not workloads:
        workloads = [LEN1]
        
    total_processed = sum(workloads)
    workload_percentages = [(w / total_processed) * 100.0 for w in workloads]
    
    mean_pct = sum(workload_percentages) / len(workload_percentages)
    variance = sum((x - mean_pct) ** 2 for x in workload_percentages) / len(workload_percentages)
    sigma_workload = math.sqrt(variance)
    
    ns_per_cell = (float(wall_time) * 1e9) / TOTAL_CELLS
    return score, cores, sigma_workload, ns_per_cell

# Initialize Master Spreadsheet
with open(OUTPUT_CSV, mode='w', newline='') as file:
    writer = csv.writer(file)
    writer.writerow([
        "Timestamp", "Hardware_Condition", "Method_Name", "Trial_ID", 
        "Alignment_Score", "Cores_Used", "Wall_Time_Sec", 
        "Sigma_Workload_Percent", "Nano_Per_Cell"
    ])

print("=========================================================================")
print("   LAUNCHING FULLY DYNAMIC MULTI-SIZE HPC EXPERIMENT HARNESS SUITE       ")
print("=========================================================================")
print("📊 Detected Metrics Matrix:")
print(f"   ➡️ Strand 1 Base Size: {LEN1} nucleotides")
print(f"   ➡️ Strand 2 Base Size: {LEN2} nucleotides")
print(f"   ➡️ Total Cells Slated : {TOTAL_CELLS:,} evaluation cells\n")

for cond_name, config in CONDITIONS.items():
    print(f"▶️ ACTIVATING RESILIENCE PHASE: [{cond_name}]")
    set_system_hardware_governor(config["governor"])
    
    for name, source_path in SOURCES.items():
        if not os.path.exists(source_path):
            print(f"  ❌ ERROR: Missing source file '{source_path}'. Skipping this track.")
            continue
            
        binary_target = f"./run_{name.lower()}_temp"
        
        print(f"  🛠️ Compiling {source_path}... ", end='', flush=True)
        try:
            compile_binary(source_path, binary_target, config["flags"])
            print("Done!")
        except subprocess.CalledProcessError:
            print(f"❌ COMPILATION FAILED for {source_path}!")
            continue
        
        print(f"  ⚡ Running Trials for Method: [{name}]")
        for trial in range(1, REPETITIONS + 1):
            print(f"     Processing Trial {trial}/{REPETITIONS}... ", end='', flush=True)
            
            try:
                result = subprocess.run(
                    [binary_target, SEQ1_PATH, SEQ2_PATH], 
                    stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, check=True
                )
                
                time_match = re.search(r"RUNNING_TIME:\s*([\d\.]+)", result.stdout)
                wall_time = float(time_match.group(1)) if time_match else 0.0
                
                score, cores, sigma_work, ns_cell = calculate_metrics(result.stdout, wall_time)
                
                timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
                with open(OUTPUT_CSV, mode='a', newline='') as file:
                    writer = csv.writer(file)
                    writer.writerow([
                        timestamp, cond_name, name, trial, 
                        score, cores, f"{wall_time:.6f}", 
                        f"{sigma_work:.2f}%", f"{ns_cell:.2f}"
                    ])
                print(f"Success! Time: {wall_time:.4f}s | Sigma: {sigma_work:.2f}% | Density: {ns_cell:.2f} ns/cell")
                
            except subprocess.CalledProcessError as e:
                print(f"❌ CRITICAL RUNTIME ERROR in binary execution.")
                print(e.stderr)
                
        if os.path.exists(binary_target):
            os.remove(binary_target)
        print() 

set_system_hardware_governor("performance")
print("=========================================================================")
print(f"🔬 ALL SWEEPS COMPLETE! Multi-size data safely saved to: {OUTPUT_CSV}")
print("=========================================================================")
