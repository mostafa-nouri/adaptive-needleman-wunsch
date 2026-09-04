#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h> 
#include <string.h>
#include <time.h>
#include <immintrin.h>

#define MATCH 1
#define MISMATCH -1
#define GAP -2

typedef struct {
	const char *sequence1;
	const char *sequence2;
	int **score;
	int len1;
	int len2;
	int thread_id;
	int core_id;          
	int thread_num;       
	
	// Coordinate structures tracking cellular assignments per diagonal line step
	int *diagonal_cells_processed;
	pthread_barrier_t *diagonal_barrier;
} ThreadData;

int maximun(int a, int b, int c) {
	if (a >= b && a >= c) return a;
	if (b >= a && b >= c) return b;
	return c;
}

char* read_fasta_dynamic(const char *filename) {
	FILE *file = fopen(filename, "r");
	if (!file) {
		perror("Cannot open file");
		exit(1);
	}

	size_t capacity = 1024;
	size_t length = 0;
	char *sequence = (char *)malloc(capacity);
	if (!sequence) {
		perror("Memory allocation failed");
		exit(1);
	}

	char line[1024]; 
	while (fgets(line, sizeof(line), file)) {
		if (line[0] == '>') continue;

		line[strcspn(line, "\n")] = '\0';

		size_t line_len = strlen(line);
		if (length + line_len + 1 > capacity) {
			capacity *= 2;
			sequence = (char *)realloc(sequence, capacity);
			if (!sequence) {
				perror("Memory reallocation failed");
				exit(1);
			}
		}

		strcpy(sequence + length, line);
		length += line_len;
	}

	sequence[length] = '\0';
	fclose(file);
	return sequence;
}

void* thread_func_paper4(void *arg) {
	ThreadData *data = (ThreadData*)arg;
	
	cpu_set_t cpuset;
	CPU_ZERO(&cpuset);
	CPU_SET(data->core_id, &cpuset);
	pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);

	int n = data->len1;
	int m = data->len2;
	int N = data->thread_num;

	// Total minor diagonals in an (n+1)x(m+1) matrix = n + m - 1
	// Index starts at 2 because the minimum boundary cell coordinate is (1,1) -> 1+1=2
	int total_diagonals = n + m;

	for (int k = 2; k <= total_diagonals; k++) {
		
		// 1. Calculate the exact mathematical boundaries of the current diagonal line step
		int min_i = (k - m > 1) ? k - m : 1;
		int max_i = (k - 1 < n) ? k - 1 : n;

		if (min_i <= max_i) {
			int diagonal_length = max_i - min_i + 1;

			// --- SECTION 4.2.1 LATENCY OPTIMIZATION ---
			// If the diagonal is shorter than half the thread count, thread management 
			// introduces more overhead than benefit. Let Thread 0 compute it sequentially.
			if (diagonal_length < (N / 2)) {
				if (data->thread_id == 0) {
					for (int i = min_i; i <= max_i; i++) {
						int j = k - i;
						
						char char1 = data->sequence1[i - 1];
						char char2 = data->sequence2[j - 1];
						int match_score = (char1 == char2) ? MATCH : MISMATCH;

						int diag = data->score[i - 1][j - 1] + match_score;
						int up   = data->score[i - 1][j] + GAP;
						int left = data->score[i][j - 1] + GAP;

						data->score[i][j] = maximun(diag, up, left);
						data->diagonal_cells_processed[0]++;
					}
				}
			} 
			// --- STANDARD PARALLEL SEGMENTED EXECUTION ---
			else {
				// Divide the current diagonal elements evenly into chunks among threads
				int base_chunk = diagonal_length / N;
				int remainder = diagonal_length % N;

				int my_start_offset = data->thread_id * base_chunk + (data->thread_id < remainder ? data->thread_id : remainder);
				int my_allocated_elements = base_chunk + (data->thread_id < remainder ? 1 : 0);

				int my_start_i = min_i + my_start_offset;
				int my_end_i = my_start_i + my_allocated_elements - 1;

				for (int i = my_start_i; i <= my_end_i; i++) {
					int j = k - i;

					// --- COMPLETELY AUTOMATED HARDWARE SIMULATION DYNAMIC LOOP LAYER ---
					#ifdef SIMULATE_DELAY
						if (data->core_id == 4 || data->core_id == 6) {
							for (volatile int spin = 0; spin < SPIN_COUNT; spin += 1) {
								_mm_pause(); 
							}
						}
					#endif

					char char1 = data->sequence1[i - 1];
					char char2 = data->sequence2[j - 1];
					int match_score = (char1 == char2) ? MATCH : MISMATCH;

					int diag = data->score[i - 1][j - 1] + match_score;
					int up   = data->score[i - 1][j] + GAP;
					int left = data->score[i][j - 1] + GAP;

					data->score[i][j] = maximun(diag, up, left);
					data->diagonal_cells_processed[data->thread_id]++;
				}
			}
		}

		// --- THE CRITICAL MINOR DIAGONAL SEPARATION BARRIER ---
		// Force all threads to block and sync at the end of every diagonal step
		pthread_barrier_wait(data->diagonal_barrier);
	}

	pthread_exit(NULL);
}

int main(int argc, char *argv[]) {
	if (argc < 3) {
		fprintf(stderr, "==================================================\n");
		fprintf(stderr, "❌ CRITICAL ERROR: Missing sequence input paths.\n");
		fprintf(stderr, "Usage: %s <sequence1_path> <sequence2_path>\n", argv[0]);
		fprintf(stderr, "==================================================\n");
		return 1;
	}

	// Dynamically capture paths straight from the command line arguments
	char *sequence1 = read_fasta_dynamic(argv[1]);
	char *sequence2 = read_fasta_dynamic(argv[2]);
	
	int len1 = strlen(sequence1);
	int len2 = strlen(sequence2);

	struct timespec start_time, end_time;
	double elapsed_time;

	int i;
	int **score = (int **)malloc((len1 + 1) * sizeof(int *));
	for (i = 0; i <= len1; i++) {
		score[i] = (int *)malloc((len2 + 1) * sizeof(int)); 
	}

	for (i = 0; i <= len1; i++) score[i][0] = i * GAP;
	for (i = 0; i <= len2; i++) score[0][i] = i * GAP;

	int total_cores = sysconf(_SC_NPROCESSORS_ONLN);
	int thread_num = total_cores; 
	if (thread_num < 1) thread_num = 1;

	// Establish POSIX barrier sync points matching our 8 physical thread cores
	pthread_barrier_t diagonal_barrier;
	pthread_barrier_init(&diagonal_barrier, NULL, thread_num);

	pthread_t threads[thread_num];
	ThreadData thread_data[thread_num];

	int *diagonal_cells_processed = (int *)calloc(thread_num, sizeof(int));

	clock_gettime(CLOCK_MONOTONIC, &start_time);

	for (i = 0; i < thread_num; i++) {
		thread_data[i].sequence1 = sequence1;
		thread_data[i].sequence2 = sequence2;
		thread_data[i].score = score;
		thread_data[i].len1 = len1;
		thread_data[i].len2 = len2;
		thread_data[i].thread_id = i;
		thread_data[i].thread_num = thread_num;
		thread_data[i].diagonal_cells_processed = diagonal_cells_processed;
		thread_data[i].diagonal_barrier = &diagonal_barrier;
		
		thread_data[i].core_id = i % total_cores;

		pthread_create(&threads[i], NULL, thread_func_paper4, (void *)&thread_data[i]);
	}

	for (i = 0; i < thread_num; i++) {
		pthread_join(threads[i], NULL);
	}

	clock_gettime(CLOCK_MONOTONIC, &end_time);
	
	elapsed_time = (end_time.tv_sec - start_time.tv_sec) + 
	               (end_time.tv_nsec - start_time.tv_nsec) / 1000000000.0;
				   
	// --- INTEGRATED PRODUCTION REGISTRY DUMP FOR HARNESS DATA PARSING ---
	printf("METHOD: PAPER4_DIAGONAL_BARRIER\n");
	printf("ALIGNMENT_SCORE: %d\n", score[len1][len2]);
	printf("CORES_USED: %d\n", thread_num);
	printf("RUNNING_TIME: %.6f\n", elapsed_time);
	
	long total_cells_computed_globally = (long)len1 * len2;
	for (i = 0; i < thread_num; i++) {
		// Convert individual vector workloads into row-equivalent ratios for parsing balance math
		double worker_ratio = (double)diagonal_cells_processed[i] / total_cells_computed_globally;
		int row_equivalent_value = (int)(worker_ratio * len1);
		printf("THREAD_WORKLOAD: Thread %d processed %d rows\n", i, row_equivalent_value);
	}

	pthread_barrier_destroy(&diagonal_barrier);
	for (i = 0; i <= len1; i++) free(score[i]);
	free(score);
	free(diagonal_cells_processed);
	free(sequence1);
	free(sequence2);

	return 0;
}
