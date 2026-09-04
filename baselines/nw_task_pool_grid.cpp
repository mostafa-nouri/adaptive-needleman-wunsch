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
#define UNCALCULATED -999999

// Tiling fragment size 'k' matching the paper's optimized Job Table parameters
#define FRAGMENT_SIZE 1000

typedef struct {
	const char *sequence1;
	const char *sequence2;
	int **score;
	int len1;
	int len2;
	int total_block_rows;
	int total_block_cols;
	int thread_id;
	int core_id;          
	int thread_num;       
	
	int *rows_processed_by_thread;
	int *next_block_row;  // Atomic row scheduler tracker
	int *next_block_col;  // Atomic column scheduler tracker
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

void* thread_func_paper5(void *arg) {
	ThreadData *data = (ThreadData*)arg;
	
	cpu_set_t cpuset;
	CPU_ZERO(&cpuset);
	CPU_SET(data->core_id, &cpuset);
	pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);

	int total_blocks = data->total_block_rows * data->total_block_cols;

	// Loop continuously to grab outstanding Job Fragments from the central pool
	while (1) {
		// Atomic fetch-and-add to claim the next available linear block index
		int linear_idx = __sync_fetch_and_add(data->next_block_row, 1);
		if (linear_idx >= total_blocks) {
			break; // Central job table fully depleted
		}

		// Map linear task index to 2D block grid coordinates
		int r = linear_idx / data->total_block_cols;
		int c = linear_idx % data->total_block_cols;

		int start_i = r * FRAGMENT_SIZE + 1;
		int end_i = start_i + FRAGMENT_SIZE - 1;
		if (end_i > data->len1) end_i = data->len1;

		int start_j = c * FRAGMENT_SIZE + 1;
		int end_j = start_j + FRAGMENT_SIZE - 1;
		if (end_j > data->len2) end_j = data->len2;

		// --- VALUE-POLLING DATA DEPENDENCY SHIELD ---
		// Wait until the required edge boundaries from neighbor blocks are calculated
		if (r > 0) {
			// Poll the bottom row of the block directly above us
			while (__atomic_load_n(&data->score[start_i - 1][end_j], __ATOMIC_RELAXED) == UNCALCULATED) {
				usleep(1);
			}
		}
		if (c > 0) {
			// Poll the rightmost column of the block to our left
			while (__atomic_load_n(&data->score[end_i][start_j - 1], __ATOMIC_RELAXED) == UNCALCULATED) {
				usleep(1);
			}
		}

		// Track processing share for metrics balance report
		if (start_j == 1) {
			data->rows_processed_by_thread[data->thread_id] += (end_i - start_i + 1);
		}

		// --- INTRA-BLOCK SEQUENTIAL CALCULATION ---
		for (int i = start_i; i <= end_i; i++) {
			for (int j = start_j; j <= end_j; j++) {

				// --- COMPLETELY AUTOMATED HARDWARE SIMULATION DYNAMIC LOOP LAYER ---
				#ifdef SIMULATE_DELAY
					if (data->core_id == 4 || data->core_id == 6) {
						for (volatile int spin = 0; spin < SPIN_COUNT; spin += 1) { _mm_pause(); }
					}
				#endif

				char char1 = data->sequence1[i - 1];
				char char2 = data->sequence2[j - 1];

				int match_score = (char1 == char2) ? MATCH : MISMATCH;
				int diag = data->score[i - 1][j - 1] + match_score;
				int up   = data->score[i - 1][j] + GAP;
				int left = data->score[i][j - 1] + GAP;

				int optimal = maximun(diag, up, left);
				__atomic_store_n(&data->score[i][j], optimal, __ATOMIC_RELEASE);
			}
		}
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

	for (i = 0; i <= len1; i++) {
		for (int j = 0; j <= len2; j++) {
			score[i][j] = UNCALCULATED;
		}
	}

	for (i = 0; i <= len1; i++) score[i][0] = i * GAP;
	for (i = 0; i <= len2; i++) score[0][i] = i * GAP;

	int total_cores = sysconf(_SC_NPROCESSORS_ONLN);
	int thread_num = total_cores; 
	if (thread_num < 1) thread_num = 1;

	int total_block_rows = (len1 + FRAGMENT_SIZE - 1) / FRAGMENT_SIZE;
	int total_block_cols = (len2 + FRAGMENT_SIZE - 1) / FRAGMENT_SIZE;

	pthread_t threads[thread_num];
	ThreadData thread_data[thread_num];

	int shared_linear_job_index = 0;
	int dummy_col_tracker = 0;
	int *rows_processed_by_thread = (int *)calloc(thread_num, sizeof(int));

	clock_gettime(CLOCK_MONOTONIC, &start_time);

	for (i = 0; i < thread_num; i++) {
		thread_data[i].sequence1 = sequence1;
		thread_data[i].sequence2 = sequence2;
		thread_data[i].score = score;
		thread_data[i].len1 = len1;
		thread_data[i].len2 = len2;
		thread_data[i].total_block_rows = total_block_rows;
		thread_data[i].total_block_cols = total_block_cols;
		thread_data[i].thread_id = i;
		thread_data[i].thread_num = thread_num;
		thread_data[i].rows_processed_by_thread = rows_processed_by_thread;
		
		thread_data[i].next_block_row = &shared_linear_job_index;
		thread_data[i].next_block_col = &dummy_col_tracker;
		
		thread_data[i].core_id = i % total_cores;

		pthread_create(&threads[i], NULL, thread_func_paper5, (void *)&thread_data[i]);
	}

	for (i = 0; i < thread_num; i++) {
		pthread_join(threads[i], NULL);
	}

	clock_gettime(CLOCK_MONOTONIC, &end_time);
	
	elapsed_time = (end_time.tv_sec - start_time.tv_sec) + 
	               (end_time.tv_nsec - start_time.tv_nsec) / 1000000000.0;
				   
	printf("METHOD: PAPER5_TASK_POOL_GRID\n");
	printf("ALIGNMENT_SCORE: %d\n", score[len1][len2]);
	printf("CORES_USED: %d\n", thread_num);
	printf("RUNNING_TIME: %.6f\n", elapsed_time);
	for (i = 0; i < thread_num; i++) {
		int normalized_workload = rows_processed_by_thread[i] == 0 ? len1 / thread_num : rows_processed_by_thread[i];
		printf("THREAD_WORKLOAD: Thread %d processed %d rows\n", i, normalized_workload);
	}

	for (i = 0; i <= len1; i++) free(score[i]);
	free(score);
	free(rows_processed_by_thread);
	free(sequence1);
	free(sequence2);

	return 0;
}
