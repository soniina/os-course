#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

#define BASE_DECIMAL 10

void generate_matrix(double* matrix, int size) {
  for (int i = 0; i < size * size; i++) {
    matrix[i] = (double)random() / RAND_MAX;
  }
}

void multiply_matrices(
    const double* matrix1, const double* matrix2, double* result, int size
) {
  for (int i = 0; i < size; i++) {
    for (int j = 0; j < size; j++) {
      double sum = 0.0;
      for (int k = 0; k < size; k++) {
        sum += matrix1[i * size + k] * matrix2[k * size + j];
      }
      result[i * size + j] = sum;
    }
  }
}

int main(int argc, char* argv[]) {
  int size = 0;
  int iterations = 1;

  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--size") == 0) {
      char* endptr = 0;
      size = (int)strtol(argv[++i], &endptr, BASE_DECIMAL);
      if (*endptr != '\0') {
        (void)fprintf(stderr, "Invalid size: %s\n", argv[i]);
        return 1;
      }
    } else if (strcmp(argv[i], "--iterations") == 0) {
      char* endptr = 0;
      iterations = (int)strtol(argv[++i], &endptr, BASE_DECIMAL);
      if (*endptr != '\0') {
        (void)fprintf(stderr, "Invalid iterations: %s\n", argv[i]);
        return 1;
      }
    }
  }

  if (size <= 0) {
    (void)fprintf(
        stderr,
        "Usage: %s --size <matrix_size> [--iterations <count>]\n",
        argv[0]
    );
    return 1;
  }

  srandom(time(NULL));

  size_t total_size = (size_t)size * (size_t)size * sizeof(double);
  double* matrix1 = (double*)malloc(total_size);
  double* matrix2 = (double*)malloc(total_size);
  double* result = (double*)malloc(total_size);

  if (matrix1 == NULL || matrix2 == NULL || result == NULL) {
    perror("malloc failed");
    return 1;
  }

  for (int iter = 0; iter < iterations; iter++) {
    generate_matrix(matrix1, size);
    generate_matrix(matrix2, size);
    multiply_matrices(matrix1, matrix2, result, size);
  }

  free(matrix1);
  free(matrix2);
  free(result);

  return 0;
}