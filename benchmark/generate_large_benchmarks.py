import random
import os

# Configuration Knobs
# Defines the exact 10 scaling milestones needed for your O(N) line chart
SCALING_THRESHOLDS = [50000, 60000, 70000, 80000, 90000, 100000, 120000, 150000, 200000, 250000]
DATASET_DIR = "../dataset"

# Ensure the centralized repository dataset directory exists
os.makedirs(DATASET_DIR, exist_ok=True)

def generate_synthetic_dna(length, mutation_rate=0.02):
    """Generates a base sequence and a mutated counterpart string."""
    bases = ['A', 'C', 'G', 'T']
    # Create the baseline master sequence
    seq1 = ''.join(random.choice(bases) for _ in range(length))
    
    # Introduce random mutations, insertions, or deletions to mimic real DNA
    seq2_list = []
    for base in seq1:
        if random.random() < mutation_rate:
            change_type = random.choice(['mismatch', 'insert', 'delete'])
            if change_type == 'mismatch':
                seq2_list.append(random.choice([b for b in bases if b != base]))
            elif change_type == 'insert':
                seq2_list.append(base)
                seq2_list.append(random.choice(bases))
            elif change_type == 'delete':
                continue  # Skip writing the base entirely
        else:
            seq2_list.append(base)
            
    return seq1, ''.join(seq2_list)

print("=========================================================================")
print(" 🧬 LAUNCHING AUTOMATED MULTI-SIZE GEOMETRIC DATASET GENERATOR          ")
print("=========================================================================")

# Loop through every target threshold to generate all 10 pairs of files automatically
for target_len in SCALING_THRESHOLDS:
    print(f" ▶️ Generating paired strands for milestone: {target_len:,} nucleotides... ", end='', flush=True)
    
    seq1, seq2 = generate_synthetic_dna(target_len)
    
    # Convert length tokens into standard structural names (e.g., seq1_50.fasta)
    len_token = str(target_len // 1000)
    file1_path = os.path.join(DATASET_DIR, f"seq1_{len_token}.fasta")
    file2_path = os.path.join(DATASET_DIR, f"seq2_{len_token}.fasta")
    
    # Write out Sequence 1 (Reference)
    with open(file1_path, "w") as f:
        f.write(f">Reference_Strand_Milestone_{len_token}K_Length_{len(seq1)}\n{seq1}\n")
        
    # Write out Sequence 2 (Mutated Counterpart)
    with open(file2_path, "w") as f:
        f.write(f">Mutated_Strand_Milestone_{len_token}K_Length_{len(seq2)}\n{seq2}\n")
        
    print(f"Done! ({len(seq1):,} vs {len(seq2):,} bases)")

print("=========================================================================")
print(f" ✅ ALL 10 GEOMETRIC DATASETS COMPLIANT AND SECURED INSIDE: {DATASET_DIR}")
print("=========================================================================")
