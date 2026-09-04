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
	int *sharedRow;       
	int *completedRows;
	int thread_id;
	int core_id;          
	int thread_num;       
	int max_chunk_ceiling; 
	
	int *row_progress;
	double *core_speeds;  
	int *rows_processed_by_thread; 
	int allocation_chunk; 
	
	pthread_mutex_t *row_mutexes;
	pthread_cond_t *row_conds;
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

void* thread_func(void *arg) {
	ThreadData *data = (ThreadData*)arg;
	
	cpu_set_t cpuset;
	CPU_ZERO(&cpuset);
	CPU_SET(data->core_id, &cpuset);
	pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);

	data->allocation_chunk = 16; 
	int chunk_size = data->allocation_chunk;
	
	int top_row = __sync_fetch_and_add(data->sharedRow, chunk_size);

	double total_microseconds_spent = 0.0;
	long total_rows_processed_by_me = 0;

	while (top_row < data->len1 + 1) {
		int bottom_row = top_row + chunk_size - 1;
		int current_band_size = chunk_size;

		if (bottom_row > data->len1) {
			bottom_row = data->len1;
			current_band_size = bottom_row - top_row + 1;
		}

		data->rows_processed_by_thread[data->thread_id] += current_band_size;
		total_rows_processed_by_me += current_band_size; 

		double pure_calc_duration = 0.0;

		for (int j = 1; j <= data->len2; j++) {
			
			if (top_row > 1) {
				pthread_mutex_lock(&data->row_mutexes[top_row - 1]);
				while (__atomic_load_n(&data->row_progress[top_row - 1], __ATOMIC_RELAXED) < j) {
					pthread_cond_wait(&data->row_conds[top_row - 1], &data->row_mutexes[top_row - 1]);
				}
				pthread_mutex_unlock(&data->row_mutexes[top_row - 1]);
			}

			struct timespec start_calc, end_calc;
			clock_gettime(CLOCK_MONOTONIC, &start_calc);

			for (int r = 0; r < current_band_size; r++) {
				int curr_row_idx = top_row + r;

				// --- COMPLETELY AUTOMATED HARDWARE SIMULATION DYNAMIC LOOP LAYER ---
				#ifdef SIMULATE_DELAY
					if (data->core_id == 4 || data->core_id == 6) {
						// SPIN_COUNT is passed dynamically from the python harness g++ command line
						for (volatile int spin = 0; spin < SPIN_COUNT; spin += 1) {
							_mm_pause(); 
						}
					}
				#endif

				char char1 = data->sequence1[curr_row_idx - 1];
				char char2 = data->sequence2[j - 1];

				int match_score = (char1 == char2) ? MATCH : MISMATCH;
				int diag = data->score[curr_row_idx - 1][j - 1] + match_score;
				int up   = data->score[curr_row_idx - 1][j] + GAP;
				int left = data->score[curr_row_idx][j - 1] + GAP;

				data->score[curr_row_idx][j] = (diag > up) ? 
									((diag > left) ? diag : left) : 
									((up > left) ? up : left);

				if (curr_row_idx == bottom_row) {
					pthread_mutex_lock(&data->row_mutexes[bottom_row]);
					__atomic_store_n(&data->row_progress[bottom_row], j, __ATOMIC_RELEASE);
					pthread_cond_signal(&data->row_conds[bottom_row]);
					pthread_mutex_unlock(&data->row_mutexes[bottom_row]);
				}
			}

			clock_gettime(CLOCK_MONOTONIC, &end_calc);
			pure_calc_duration += (end_calc.tv_sec - start_calc.tv_sec) * 1000000.0 + 
			                      (end_calc.tv_nsec - start_calc.tv_nsec) / 1000.0;
		}

		total_microseconds_spent += pure_calc_duration;
		double microseconds_per_row = total_microseconds_spent / total_rows_processed_by_me;
		
		data->core_speeds[data->thread_id] = microseconds_per_row;
		__sync_synchronize();

		double total_active_speed_sum = 0.0;
		int active_threads_counted = 0;

		for (int t = 0; t < data->thread_num; t++) {
			double s = data->core_speeds[t];
			if (s > 0.0) {
				total_active_speed_sum += s;
				active_threads_counted++;
			}
		}

		double global_average_speed = 0.0;
		if (active_threads_counted > 0) {
			global_average_speed = total_active_speed_sum / active_threads_counted;
		}

		if (active_threads_counted > 1 && global_average_speed > 0.001) {
			double performance_deviation = global_average_speed / microseconds_per_row;
			int target_chunk;

			if (performance_deviation >= 0.90) {
				target_chunk = (int)((double)data->max_chunk_ceiling * performance_deviation);
			} else {
				target_chunk = (int)(16.0 * performance_deviation);
			}
			
			if (target_chunk < 2) target_chunk = 2; 
			if (target_chunk > data->max_chunk_ceiling) target_chunk = data->max_chunk_ceiling;
			data->allocation_chunk = target_chunk;
		} else {
			data->allocation_chunk = (data->max_chunk_ceiling / 2 > 1) ? data->max_chunk_ceiling / 2 : 1;
		}

		__sync_add_and_fetch(data->completedRows, current_band_size);

		chunk_size = data->allocation_chunk;
		top_row = __sync_fetch_and_add(data->sharedRow, chunk_size);
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

	int *row_progress = NULL;
	if (posix_memalign((void**)&row_progress, 64, (len1 + 1) * sizeof(int)) != 0) {
		perror("Cache-aligned allocation failed");
		exit(1);
	}

	row_progress[0] = len2; 
	for (i = 1; i <= len1; i++) row_progress[i] = 0;

	pthread_mutex_t *row_mutexes = (pthread_mutex_t *)malloc((len1 + 1) * sizeof(pthread_mutex_t));
	pthread_cond_t *row_conds = (pthread_cond_t *)malloc((len1 + 1) * sizeof(pthread_cond_t));
	for (i = 0; i <= len1; i++) {
		pthread_mutex_init(&row_mutexes[i], NULL);
		pthread_cond_init(&row_conds[i], NULL);
	}

	int dynamic_ceiling = len2 / (thread_num * 4);
	if (dynamic_ceiling < 16)  dynamic_ceiling = 16;
	if (dynamic_ceiling > 256) dynamic_ceiling = 256; 

	pthread_t threads[thread_num];
	ThreadData thread_data[thread_num];

	int sharedRow = 1; 
	int completedRows = 0;
	
	double *core_speeds = (double *)calloc(thread_num, sizeof(double));
	int *rows_processed_by_thread = (int *)calloc(thread_num, sizeof(int));

	clock_gettime(CLOCK_MONOTONIC, &start_time);

	for (i = 0; i < thread_num; i++) {
		thread_data[i].sequence1 = sequence1;
		thread_data[i].sequence2 = sequence2;
		thread_data[i].score = score;
		thread_data[i].len1 = len1;
		thread_data[i].len2 = len2;
		thread_data[i].sharedRow = &sharedRow;
		thread_data[i].completedRows = &completedRows;
		thread_data[i].thread_id = i;
		thread_data[i].thread_num = thread_num;
		thread_data[i].max_chunk_ceiling = dynamic_ceiling;
		thread_data[i].row_mutexes = row_mutexes;
		thread_data[i].row_conds = row_conds;
		thread_data[i].core_speeds = core_speeds;
		thread_data[i].rows_processed_by_thread = rows_processed_by_thread;
		thread_data[i].row_progress = row_progress;
		
		thread_data[i].core_id = i % total_cores;

		pthread_create(&threads[i], NULL, thread_func, (void *)&thread_data[i]);
	}

	for (i = 0; i < thread_num; i++) {
		pthread_join(threads[i], NULL);
	}

	clock_gettime(CLOCK_MONOTONIC, &end_time);
	
	elapsed_time = (end_time.tv_sec - start_time.tv_sec) + 
	               (end_time.tv_nsec - start_time.tv_nsec) / 1000000000.0;
				   
	// --- STANDARDIZED INTEGRATED OUTPUT REGISTRY FOR PYTHON PARSING ---
	printf("METHOD: %s\n", "ADAPTIVE");
	printf("ALIGNMENT_SCORE: %d\n", score[len1][len2]);
	printf("CORES_USED: %d\n", thread_num);
	printf("RUNNING_TIME: %.6f\n", elapsed_time);
	for (i = 0; i < thread_num; i++) {
		printf("THREAD_WORKLOAD: Thread %d processed %d rows\n", i, rows_processed_by_thread[i]);
	}

	for (i = 0; i <= len1; i++) {
		pthread_mutex_destroy(&row_mutexes[i]);
		pthread_cond_destroy(&row_conds[i]);
		free(score[i]);
	}

	free(row_mutexes);
	free(row_conds);
	free(core_speeds);
	free(rows_processed_by_thread);
	free(row_progress);
	free(score);
	free(sequence1);
	free(sequence2);

	return 0;
}
