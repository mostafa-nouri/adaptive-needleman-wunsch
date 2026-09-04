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
	int start_row;         
	int end_row;           
	int thread_id;
	int core_id;           
	int thread_num;        
	
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

void* thread_func_paper3_fixed(void *arg) {
	ThreadData *data = (ThreadData*)arg;
	
	cpu_set_t cpuset;
	CPU_ZERO(&cpuset);
	CPU_SET(data->core_id, &cpuset);
	pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);

	int total_rows_to_do = data->end_row - data->start_row + 1;

	// Columns are the outer loop to maximize spatial memory locality
	for (int j = 1; j <= data->len2; j++) {
		
		// --- FIX: POll ONLY ONCE PER WHOLE COLUMN AT THE CHUNK BOUNDARY ---
		if (data->start_row > 1) {
			// Wait until the thread handling the block above has completed column j
			while (__atomic_load_n(&data->row_progress[data->start_row - 1], __ATOMIC_RELAXED) < j) {
				_mm_pause(); // Hardware spin-wait prevents bus trashing
			}
		}

		// Process the entire block allocation smoothly and lock-free
		for (int i = data->start_row; i <= data->end_row; i++) {
			
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
		}
		
		// Signal to the thread below that our entire chunk has crossed column j
		__atomic_store_n(&data->row_progress[data->end_row], j, __ATOMIC_RELEASE);
		data->rows_processed_by_thread[data->thread_id] += total_rows_to_do;
	}

	pthread_exit(NULL);
}

int main(int argc, char *argv[]) {
	if (argc < 3) {
		fprintf(stderr, "❌ CRITICAL ERROR: Missing sequence input paths.\n");
		fprintf(stderr, "Usage: %s <sequence1_path> <sequence2_path>\n", argv[0]);
		return 1;
	}

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
	for (int j = 0; j <= len2; j++) score[0][j] = j * GAP;

	int total_cores = sysconf(_SC_NPROCESSORS_ONLN);
	int thread_num = total_cores; 
	if (thread_num < 1) thread_num = 1;

	int *row_progress = (int *)calloc(len1 + 1, sizeof(int));
	for (i = 0; i <= len1; i++) row_progress[i] = 0;
	row_progress[0] = len2; // Row 0 boundary is pre-calculated baseline

	pthread_t threads[thread_num];
	ThreadData thread_data[thread_num];

	int *rows_processed_by_thread = (int *)calloc(thread_num, sizeof(int));
	
	int base_chunk = len1 / thread_num;
	int remainder = len1 % thread_num;
	int current_row = 1;

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
		thread_data[i].row_progress = row_progress;
		
		thread_data[i].core_id = i % total_cores;

		thread_data[i].start_row = current_row;
		int allocated_rows = base_chunk + (i < remainder ? 1 : 0);
		thread_data[i].end_row = current_row + allocated_rows - 1;
		current_row = thread_data[i].end_row + 1;

		pthread_create(&threads[i], NULL, thread_func_paper3_fixed, (void *)&thread_data[i]);
	}

	for (i = 0; i < thread_num; i++) {
		pthread_join(threads[i], NULL);
	}

	clock_gettime(CLOCK_MONOTONIC, &end_time);
	
	elapsed_time = (end_time.tv_sec - start_time.tv_sec) + 
	               (end_time.tv_nsec - start_time.tv_nsec) / 1000000000.0;
				   
	printf("METHOD: PAPER3_STATIC_STAGGERED_BARRIER\n");
	printf("ALIGNMENT_SCORE: %d\n", score[len1][len2]);
	printf("CORES_USED: %d\n", thread_num);
	printf("RUNNING_TIME: %.6f\n", elapsed_time);
	for (i = 0; i < thread_num; i++) {
		int normalized_workload = rows_processed_by_thread[i] / len2;
		printf("THREAD_WORKLOAD: Thread %d processed %d rows\n", i, normalized_workload);
	}

	for (i = 0; i <= len1; i++) free(score[i]);
	free(score);
	free(row_progress);
	free(rows_processed_by_thread);
	free(sequence1);
	free(sequence2);

	return 0;
}
