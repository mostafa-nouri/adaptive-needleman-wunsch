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

	// --- 1. FULL QUADRATIC MATRIX ALLOCATION O(MN) ---
	int **score = (int **)malloc((len1 + 1) * sizeof(int *));
	for (int i = 0; i <= len1; i++) {
		score[i] = (int *)malloc((len2 + 1) * sizeof(int));
	}

	clock_gettime(CLOCK_MONOTONIC, &start_time);

	// Initialize matrix base borders
	for (int i = 0; i <= len1; i++) score[i][0] = i * GAP;
	for (int j = 0; j <= len2; j++) score[0][j] = j * GAP;

	// --- 2. SEQUENTIAL FORWARD CELL SCORING PASS ---
	for (int i = 1; i <= len1; i++) {
		for (int j = 1; j <= len2; j++) {
			char char1 = sequence1[i - 1];
			char char2 = sequence2[j - 1];
			int match_score = (char1 == char2) ? MATCH : MISMATCH;

			int diag = score[i - 1][j - 1] + match_score;
			int up   = score[i - 1][j] + GAP;
			int left = score[i][j - 1] + GAP;

			score[i][j] = maximun(diag, up, left);
		}
	}

	// --- 3. CLASSIC BACKWARD MATRIX TRACEBACK LOOP ---
	// Create larger strings to append aligned outputs safely
	char *align1 = (char *)malloc((len1 + len2 + 1) * sizeof(char));
	char *align2 = (char *)malloc((len1 + len2 + 1) * sizeof(char));
	int idx = 0;

	int i = len1;
	int j = len2;

	while (i > 0 || j > 0) {
		if (i > 0 && j > 0) {
			char char1 = sequence1[i - 1];
			char char2 = sequence2[j - 1];
			int match_score = (char1 == char2) ? MATCH : MISMATCH;

			if (score[i][j] == score[i - 1][j - 1] + match_score) {
				align1[idx] = sequence1[i - 1];
				align2[idx] = sequence2[j - 1];
				i--; j--; idx++;
				continue;
			}
		}
		if (i > 0 && (j == 0 || score[i][j] == score[i - 1][j] + GAP)) {
			align1[idx] = sequence1[i - 1];
			align2[idx] = '-';
			i--; idx++;
		} else if (j > 0) {
			align1[idx] = '-';
			align2[idx] = sequence2[j - 1];
			j--; idx++;
		}
	}
	align1[idx] = '\0';
	align2[idx] = '\0';

	clock_gettime(CLOCK_MONOTONIC, &end_time);
	elapsed_time = (end_time.tv_sec - start_time.tv_sec) + (end_time.tv_nsec - start_time.tv_nsec) / 1e9;
				   
	printf("METHOD: CLASSIC_QUADRATIC_TRACEBACK\n");
	printf("ALIGNMENT_SCORE: %d\n", score[len1][len2]);
	printf("CORES_USED: 1\n");
	printf("TOTAL_ALIGNED_LENGTH: %d\n", idx);
	printf("RUNNING_TIME: %.6f\n", elapsed_time);

	// Clean up heap structures safely
	free(align1);
	free(align2);
	for (int row = 0; row <= len1; row++) free(score[row]);
	free(score);
	free(sequence1);
	free(sequence2);
	return 0;
}
