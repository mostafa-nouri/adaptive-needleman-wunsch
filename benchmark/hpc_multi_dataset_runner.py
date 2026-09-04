import subprocess
import re
import os
import csv
from datetime import datetime

# --- CONFIGURATION LAYER ---
# Shared testing directory configurations
DATASET_DIR = "../dataset"
SIZES = ["50"]#, "60", "70", "80", "90", "100", "120", "150", "200", "250"]

# Centralized Multi-Engine Configuration Tracking Block
EVALUATION_SUITE = [
    {
        "framework_id": "CLASSIC_SEQUENTIAL",
        "source_path": "../src/classic/nw_classic_sequential_linear_space.cpp",
        "temp_binary": "./bin_classic_temp",
        "output_csv": "multi_dataset_scalability_classic_report.csv"
    },
    {
        "framework_id": "OUR_ADAPTIVE_PARALLEL",
        "source_path": "../src/pals_engine/nw_adaptive_linear_space.cpp",
        "temp_binary": "./bin_pals_temp",
        "output_csv": "multi_dataset_scalability_report.csv"
    }
]

# Programmatically build the 10 scaling dataset milestones
DATASET_MATRIX = [
    {
        "name": f"Genomic Scale Sweep ({size}K bases)",
        "seq1": os.path.join(DATASET_DIR, f"seq1_{size}.fasta"),
        "seq2": os.path.join(DATASET_DIR, f"seq2_{size}.fasta")
    }
    for size in SIZES
]

def get_fasta_length(fasta_path):
    """Parses a FASTA file on the fly and counts its total base characters."""
    if not os.path.exists(fasta_path):
        return None
    length = 0
    with open(fasta_path, 'r') as f:
        for line in f:
            if line.startswith('>'):
                continue
            length += len(line.strip())
    return length

def compile_optimized_binary(source_file, binary_name):
    """Compiles the targeted C++ file with aggressive native -O3 optimizations."""
    print(f"  🛠️  Compiling {source_file} -> {binary_name} with -O3 flag... ", end='', flush=True)
    compile_cmd = ["g++", "-O3", "-lpthread", source_file, "-o", binary_name]
    subprocess.run(compile_cmd, check=True)
    print("Done!")

print("=========================================================================")
print("   LAUNCHING UNIFIED SCALABILITY SWEEPER FOR CLASSIC & PALS ENGINE       ")
print("=========================================================================")

# Step through each engine variant sequentially
for engine in EVALUATION_SUITE:
    print(f"\n▶️  STARTING EVALUATION TRACK: [{engine['framework_id']}]")
    
    # 1. Compile the framework source code safely
    if not os.path.exists(engine['source_path']):
        print(f"❌ CRITICAL ERROR: Source file not found at '{engine['source_path']}'. Skipping track.")
        continue
        
    try:
        compile_optimized_binary(engine['source_path'], engine['temp_binary'])
    except Exception as e:
        print(f"❌ COMPILATION FAILED for {engine['source_path']}: {e}")
        continue

    # 2. Initialize or wipe the dedicated spreadsheet report file
    with open(engine['output_csv'], mode='w', newline='') as file:
        writer = csv.writer(file)
        writer.writerow([
            "Timestamp", "Dataset_Name", "Len1", "Len2", "Total_Cells", 
            "Alignment_Score", "Cores_Used", "Wall_Time_Sec", "Nano_Per_Cell"
        ])

    # 3. Step through the 10 target data pairs sequentially
    for dataset in DATASET_MATRIX:
        len1 = get_fasta_length(dataset['seq1'])
        len2 = get_fasta_length(dataset['seq2'])
        
        if len1 is None or len2 is None:
            print(f"  ⚠️  SKIPPING: Missing files '{os.path.basename(dataset['seq1'])}' or '{os.path.basename(dataset['seq2'])}'")
            continue

        print(f"  📋 Evaluating Dataset: [{dataset['name']}]")
        total_cells = len1 * len2
        print(f"     ➡️  Matrix Scale: {len1:,} x {len2:,} ({total_cells:,} total evaluation cells)")
        
        # Repetition loop tracking for three statistical runs
        trial_times = []
        last_score = 0
        last_cores = 1 if engine['framework_id'] == "CLASSIC_SEQUENTIAL" else 8
        execution_failed = False
        
        for trial in range(1, 4):
            print(f"     ⚡ Executing Trial {trial}/3... ", end='', flush=True)
            try:
                result = subprocess.run(
                    [engine['temp_binary'], dataset['seq1'], dataset['seq2']],
                    stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, check=True
                )
                
                # Extract metrics via regex matching rules
                time_match = re.search(r"RUNNING_TIME:\s*([\d\.]+)", result.stdout)
                score_match = re.search(r"ALIGNMENT_SCORE:\s*(-?\d+)", result.stdout)
                cores_match = re.search(r"CORES_USED:\s*(\d+)", result.stdout)
                
                if time_match:
                    trial_times.append(float(time_match.group(1)))
                if score_match:
                    last_score = int(score_match.group(1))
                if cores_match:
                    last_cores = int(cores_match.group(1))
                    
                print(f"Done! ({float(time_match.group(1)):.4f}s)")
                
            except subprocess.CalledProcessError as e:
                print("❌ CRITICAL TRIAL FAILURE!")
                print(e.stderr)
                execution_failed = True
                break
                
        if execution_failed or not trial_times:
            print("  ❌ Skipping metrics logging due to a test runtime error.\n")
            continue
            
        # Compute statistical summary parameters
        avg_wall_time = sum(trial_times) / len(trial_times)
        avg_ns_per_cell = (avg_wall_time * 1e9) / total_cells
        timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
        
        # Record the averaged results row directly to the dedicated spreadsheet
        with open(engine['output_csv'], mode='a', newline='') as file:
            writer = csv.writer(file)
            writer.writerow([
                timestamp, dataset['name'], len1, len2, total_cells,
                last_score, last_cores, f"{avg_wall_time:.6f}", f"{avg_ns_per_cell:.2f}"
            ])
            
        print(f"     📊 Mean Averages -> Time: {avg_wall_time:.4f}s | Score: {last_score} | Density: {avg_ns_per_cell:.2f} ns/cell\n")

    # Clean up temporary compiled binary file when the current framework sweep ends
    if os.path.exists(engine['temp_binary']):
        os.remove(engine['temp_binary'])

print("=========================================================================")
print("🔬 ALL SCALABILITY DATASETS CONCLUDED FOR BOTH ALGORITHMS!")
print("   ➡️  Classic 1-Core Database Saved To: multi_dataset_scalability_classic_report.csv")
print("   ➡️  Our Parallel Method Database Saved To: multi_dataset_scalability_report.csv")
print("=========================================================================")
