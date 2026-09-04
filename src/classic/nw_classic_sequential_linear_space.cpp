#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MATCH 1
#define MISMATCH -1
#define GAP -2

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

	// O(N) Space Optimization: Allocate only two rolling row arrays globally
	int *prev_row = (int *)malloc((len2 + 1) * sizeof(int));
	int *curr_row = (int *)malloc((len2 + 1) * sizeof(int));

	clock_gettime(CLOCK_MONOTONIC, &start_time);

	// Initialize baseline row 0 boundary elements
	for (int j = 0; j <= len2; j++) {
		prev_row[j] = j * GAP;
	}

	// --- CLASSIC SEQUENTIAL FORWARD LOOP MATRIX ---
	for (int i = 1; i <= len1; i++) {
		curr_row[0] = i * GAP; // Set the zero-column boundary element element

		for (int j = 1; j <= len2; j++) {
			char char1 = sequence1[i - 1];
			char char2 = sequence2[j - 1];
			int match_score = (char1 == char2) ? MATCH : MISMATCH;

			int diag = prev_row[j - 1] + match_score;
			int up   = prev_row[j] + GAP;
			int left = curr_row[j - 1] + GAP;

			curr_row[j] = maximun(diag, up, left);
		}

		// Pointer swap shifts rolling double rows down
		int *temp = prev_row;
		prev_row = curr_row;
		curr_row = temp;
	}

	clock_gettime(CLOCK_MONOTONIC, &end_time);
	elapsed_time = (end_time.tv_sec - start_time.tv_sec) + (end_time.tv_nsec - start_time.tv_nsec) / 1e9;
				   
	printf("METHOD: CLASSIC_SEQUENTIAL\n");
	printf("ALIGNMENT_SCORE: %d\n", prev_row[len2]);
	printf("CORES_USED: 1\n");
	printf("RUNNING_TIME: %.6f\n", elapsed_time);

	free(prev_row);
	free(curr_row);
	free(sequence1);
	free(sequence2);
	return 0;
}
