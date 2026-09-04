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

typedef struct {
	const char *sequence1;
	const char *sequence2;
	int len1;
	int len2;
	int *sharedRow;       
	int *completedRows;
	int thread_id;
	int core_id;          
	int thread_num;       
	int max_chunk_ceiling; 
	
	int **global_rows;     // Shared memory array passing row boundaries between chunks
	int *row_progress;
	int *rows_processed_by_thread; 
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

	// FIX 1: Properly allocated string array line buffer buffer
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

void* thread_func_linear(void *arg) {
	ThreadData *data = (ThreadData*)arg;
	
	cpu_set_t cpuset;
	CPU_ZERO(&cpuset);
	CPU_SET(data->core_id, &cpuset);
	pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);

	int n = data->len2;
	// Local thread buffers insulated from glibc heap manager locks
	int *local_prev = (int *)malloc((n + 1) * sizeof(int));
	int *local_curr = (int *)malloc((n + 1) * sizeof(int));

	int chunk_size = 16;
	int top_row = __sync_fetch_and_add(data->sharedRow, chunk_size);

	while (top_row < data->len1 + 1) {
		int bottom_row = top_row + chunk_size - 1;
		if (bottom_row > data->len1) bottom_row = data->len1;
		int current_band_size = bottom_row - top_row + 1;

		data->rows_processed_by_thread[data->thread_id] += current_band_size;

		// 1. Wait until the preceding chunk boundary row is ready
		while (__atomic_load_n(&data->row_progress[top_row - 1], __ATOMIC_RELAXED) < n) {
			_mm_pause();
		}
		
		// Copy the preceding chunk boundary safely into local L1 cache
		memcpy(local_prev, data->global_rows[top_row - 1], (n + 1) * sizeof(int));

		// ACTIVE EVICTION: Cleanly free top_row - 1 from RAM as its dependencies are fully exhausted
		if (top_row > 1) {
			int *row_to_free = data->global_rows[top_row - 1];
			if (row_to_free != NULL) {
				data->global_rows[top_row - 1] = NULL;
				free(row_to_free);
			}
		}

		// 2. Compute the entire chunk block allocation lock-free and malloc-free
		for (int curr_row = top_row; curr_row <= bottom_row; curr_row++) {
			// FIX 2: Explicitly index index 0 to eliminate integer-to-pointer casting errors
			local_curr[0] = curr_row * GAP;

			for (int j = 1; j <= n; j++) {
				#ifdef SIMULATE_DELAY
					if (data->core_id == 4 || data->core_id == 6) {
						for (volatile int spin = 0; spin < SPIN_COUNT; spin += 1) { _mm_pause(); }
					}
				#endif

				char char1 = data->sequence1[curr_row - 1];
				char char2 = data->sequence2[j - 1];
				int match_score = (char1 == char2) ? MATCH : MISMATCH;

				int diag = local_prev[j - 1] + match_score;
				int up   = local_prev[j] + GAP;
				int left = local_curr[j - 1] + GAP;

				local_curr[j] = (diag > up) ? ((diag > left) ? diag : left) : ((up > left) ? up : left);
			}

			// Local pointer flip shifts rolling double buffers down
			int *temp = local_prev;
			local_prev = local_curr;
			local_curr = temp;
		}

		// 3. CHUNK OVERHEAD BOUNDING: Allocate exactly ONE shared buffer per entire block sequence
		int *global_chunk_boundary = (int *)malloc((n + 1) * sizeof(int));
		memcpy(global_chunk_boundary, local_prev, (n + 1) * sizeof(int));
		
		data->global_rows[bottom_row] = global_chunk_boundary;
		__atomic_store_n(&data->row_progress[bottom_row], n, __ATOMIC_RELEASE);

		top_row = __sync_fetch_and_add(data->sharedRow, chunk_size);
	}

	free(local_prev);
	free(local_curr);
	pthread_exit(NULL);
}

int main(int argc, char *argv[]) {
	if (argc < 3) {
		fprintf(stderr, "❌ CRITICAL ERROR: Missing sequence input paths.\n");
		fprintf(stderr, "Usage: %s <sequence1_path> <sequence2_path>\n", argv[0]);
		return 1;
	}

	// FIX 3: Correctly explicit pass individual row string indices from argv
	char *sequence1 = read_fasta_dynamic(argv[1]);
	char *sequence2 = read_fasta_dynamic(argv[2]);
	
	int len1 = strlen(sequence1);
	int len2 = strlen(sequence2);

	struct timespec start_time, end_time;
	double elapsed_time;

	int total_cores = sysconf(_SC_NPROCESSORS_ONLN);
	int thread_num = total_cores;

	// Global pointer array tracking active wavefront boundaries
	int **global_rows = (int **)calloc(len1 + 1, sizeof(int *));
	global_rows[0] = (int *)malloc((len2 + 1) * sizeof(int));
	for (int j = 0; j <= len2; j++) {
		global_rows[0][j] = j * GAP;
	}

	int *row_progress = (int *)calloc(len1 + 1, sizeof(int));
	row_progress[0] = len2; 

	pthread_t threads[thread_num];
	ThreadData thread_data[thread_num];

	int sharedRow = 1; 
	int completedRows = 0;
	int *rows_processed_by_thread = (int *)calloc(thread_num, sizeof(int));

	clock_gettime(CLOCK_MONOTONIC, &start_time);

	for (int i = 0; i < thread_num; i++) {
		thread_data[i].sequence1 = sequence1;
		thread_data[i].sequence2 = sequence2;
		thread_data[i].len1 = len1;
		thread_data[i].len2 = len2;
		thread_data[i].sharedRow = &sharedRow;
		thread_data[i].completedRows = &completedRows;
		thread_data[i].thread_id = i;
		thread_data[i].thread_num = thread_num;
		
		thread_data[i].global_rows = global_rows;
		thread_data[i].row_progress = row_progress;
		thread_data[i].rows_processed_by_thread = rows_processed_by_thread;
		thread_data[i].core_id = i % total_cores;

		pthread_create(&threads[i], NULL, thread_func_linear, (void *)&thread_data[i]);
	}

	for (int i = 0; i < thread_num; i++) {
		pthread_join(threads[i], NULL);
	}

	clock_gettime(CLOCK_MONOTONIC, &end_time);
	elapsed_time = (end_time.tv_sec - start_time.tv_sec) + (end_time.tv_nsec - start_time.tv_nsec) / 1e9;
	
	// Wait safely for the absolute final matrix row to exit execution
	while (__atomic_load_n(&row_progress[len1], __ATOMIC_RELAXED) < len2) {
		_mm_pause();
	}
	int final_alignment_score = global_rows[len1][len2];
				   
	printf("METHOD: OUR_ADAPTIVE_HYSTERESIS\n");
	printf("ALIGNMENT_SCORE: %d\n", final_alignment_score);
	printf("CORES_USED: %d\n", thread_num);
	printf("RUNNING_TIME: %.6f\n", elapsed_time);

	if (global_rows[len1] != NULL) free(global_rows[len1]);
	free(global_rows);
	free(row_progress);
	free(rows_processed_by_thread);
	free(sequence1);
	free(sequence2);
	return 0;
}
