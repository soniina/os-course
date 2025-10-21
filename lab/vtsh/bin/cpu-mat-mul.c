#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

#define BASE_DECIMAL 10

void generate_matrix(double* matrix, size_t size) {
  for (int i = 0; i < size * size; i++) {
    matrix[i] = (double)random() / RAND_MAX;
  }
}

void multiply_matrices(
    const double* matrix1, const double* matrix2, double* result, size_t size
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

static int parse_arguments(
    int argc, char* argv[], size_t* size, long* iterations
) {
  for (int i = 1; i < argc; i++) {
    char* key = argv[i++];
    if (i >= argc) {
      (void)fprintf(stderr, "Missing value for --%s\n", key);
      return -1;
    }
    char* value = argv[i];

    if (strcmp(key, "--size") == 0) {
      char* endptr = NULL;
      long tmp_size = strtol(value, &endptr, BASE_DECIMAL);
      if (*endptr != '\0' || tmp_size <= 0) {
        (void)fprintf(stderr, "Invalid size: %s\n", value);
        return -1;
      }
      *size = (size_t)tmp_size;
    } else if (strcmp(key, "--iterations") == 0) {
      char* endptr = NULL;
      long tmp_iterations = strtol(value, &endptr, BASE_DECIMAL);
      if (*endptr != '\0' || tmp_iterations <= 0) {
        (void)fprintf(stderr, "Invalid iterations: %s\n", value);
        return -1;
      }
      *iterations = tmp_iterations;
    } else {
      (void)fprintf(stderr, "Unknown parameter: %s\n", key);
      return -1;
    }
  }
  return 0;
}

int main(int argc, char* argv[]) {
  size_t size = 0;
  long iterations = 1;

  if (argc <= 1 || parse_arguments(argc, argv, &size, &iterations) != 0) {
    (void)fprintf(
        stderr,
        "Usage: %s --size <matrix_size> [--iterations <count>]\n",
        argv[0]
    );
    return 1;
  }

  srandom(time(NULL));

  size_t total_size = size * size * sizeof(double);
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