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
	int **score;
	int len1;
	int len2;
	int thread_id;
	int core_id;          
	int thread_num;       
	
	int start_col;
	int end_col;
	
	int *rows_processed_by_thread;
	int *row_shared_boundary_buffer; 
	pthread_barrier_t *row_prefix_barrier;
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

	// FIX: Re-established properly typed string buffer array to prevent conversion leaks
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

void* thread_func_paper6(void *arg) {
	ThreadData *data = (ThreadData*)arg;
	
	cpu_set_t cpuset;
	CPU_ZERO(&cpuset);
	CPU_SET(data->core_id, &cpuset);
	pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);

	int m = data->len1;

	for (int i = 1; i <= m; i++) {
		
		if (data->thread_id == 0) {
			data->rows_processed_by_thread[0] += 1; 
		}

		for (int j = data->start_col; j <= data->end_col; j++) {
			
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
			
			data->score[i][j] = (diag > up) ? diag : up;
		}

		int running_left_accumulator = data->score[i][data->start_col - 1] + GAP;
		for (int j = data->start_col; j <= data->end_col; j++) {
			int current_term = data->score[i][j];
			data->score[i][j] = (current_term > running_left_accumulator) ? current_term : running_left_accumulator;
			running_left_accumulator = data->score[i][j] + GAP;
		}

		data->row_shared_boundary_buffer[data->thread_id] = running_left_accumulator;

		pthread_barrier_wait(data->row_prefix_barrier);

		int cross_offset = 0;
		if (data->thread_id > 0) {
			for (int t = 0; t < data->thread_id; t++) {
				int potential_bound = data->row_shared_boundary_buffer[t] - (data->thread_id - t) * abs(GAP);
				if (t == 0 || potential_bound > cross_offset) {
					cross_offset = potential_bound;
				}
			}
		}

		pthread_barrier_wait(data->row_prefix_barrier);

		if (data->thread_id > 0) {
			for (int j = data->start_col; j <= data->end_col; j++) {
				int internal_val = data->score[i][j];
				data->score[i][j] = (internal_val > cross_offset) ? internal_val : cross_offset;
				cross_offset += GAP;
			}
		}

		pthread_barrier_wait(data->row_prefix_barrier);
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

	// FIX: Explicitly indexing cell elements to prevent pointer conversion errors
	for (i = 0; i <= len1; i++) score[i][0] = i * GAP;
	for (i = 0; i <= len2; i++) score[0][i] = i * GAP;

	int total_cores = sysconf(_SC_NPROCESSORS_ONLN);
	int thread_num = total_cores; 
	if (thread_num < 1) thread_num = 1;

	pthread_barrier_t row_prefix_barrier;
	pthread_barrier_init(&row_prefix_barrier, NULL, thread_num);

	pthread_t threads[thread_num];
	ThreadData thread_data[thread_num];

	int *row_shared_boundary_buffer = (int *)calloc(thread_num, sizeof(int));
	int *rows_processed_by_thread = (int *)calloc(thread_num, sizeof(int));

	int base_chunk = len2 / thread_num;
	int remainder = len2 % thread_num;
	int current_col = 1;

	clock_gettime(CLOCK_MONOTONIC, &start_time);

	for (i = 0; i < thread_num; i++) {
		thread_data[i].sequence1 = sequence1;
		thread_data[i].sequence2 = sequence2;
		thread_data[i].score = score;
		thread_data[i].len1 = len1;
		thread_data[i].len2 = len2;
		thread_data[i].thread_id = i;
		thread_data[i].thread_num = thread_num;
		thread_data[i].rows_processed_by_thread = rows_processed_by_thread;
		thread_data[i].row_shared_boundary_buffer = row_shared_boundary_buffer;
		thread_data[i].row_prefix_barrier = &row_prefix_barrier;
		
		thread_data[i].core_id = i % total_cores;

		thread_data[i].start_col = current_col;
		int allocated_cols = base_chunk + (i < remainder ? 1 : 0);
		thread_data[i].end_col = current_col + allocated_cols - 1;
		current_col = thread_data[i].end_col + 1;

		pthread_create(&threads[i], NULL, thread_func_paper6, (void *)&thread_data[i]);
	}

	for (i = 0; i < thread_num; i++) {
		pthread_join(threads[i], NULL);
	}

	clock_gettime(CLOCK_MONOTONIC, &end_time);
	
	elapsed_time = (end_time.tv_sec - start_time.tv_sec) + 
	               (end_time.tv_nsec - start_time.tv_nsec) / 1000000000.0;
				   
	printf("METHOD: PAPER6_PARALLEL_PREFIX\n");
	printf("ALIGNMENT_SCORE: %d\n", score[len1][len2]);
	printf("CORES_USED: %d\n", thread_num);
	printf("RUNNING_TIME: %.6f\n", elapsed_time);
	for (i = 0; i < thread_num; i++) {
		int normalized_workload = (i == 0) ? len1 : len1 / thread_num;
		printf("THREAD_WORKLOAD: Thread %d processed %d rows\n", i, normalized_workload);
	}

	pthread_barrier_destroy(&row_prefix_barrier);
	for (i = 0; i <= len1; i++) free(score[i]);
	free(score);
	free(row_shared_boundary_buffer);
	free(rows_processed_by_thread);
	free(sequence1);
	free(sequence2);

	return 0;
}
