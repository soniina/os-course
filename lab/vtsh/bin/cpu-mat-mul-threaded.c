#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#define BASE_DECIMAL 10
#define BUFFER_SIZE 1024
#define DEFAULT_OUTPUT_FILENAME "mul_result.txt"

typedef struct {
  char** matrix1;
  char** matrix2;
  char** result;
} file_args_t;

typedef struct {
  size_t size;
  char* matrix1_file;
  char* matrix2_file;
  char* output;
  long iterations;
} thread_args_t;

double* load_matrix(const char* filename, size_t size) {
  FILE* file = fopen(filename, "r");
  if (file == NULL) {
    perror(filename);
    return NULL;
  }

  double* matrix = malloc(size * size * sizeof(double));
  if (matrix == NULL) {
    perror("malloc failed");
    (void)fclose(file);
    return NULL;
  }

  char buffer[BUFFER_SIZE];
  size_t count = 0;
  long file_pos = 0;
  while (count < size * size) {
    if (fgets(buffer, sizeof(buffer), file) == NULL) {
      if (fseek(file, 0, SEEK_SET) != 0) {
        perror("fseek failed");
        free(matrix);
        (void)fclose(file);
        return NULL;
      }
      file_pos = 0;
      continue;
    }

    char* line_ptr = buffer;
    while (*line_ptr != '\0' && count < size * size) {
      while (*line_ptr == ' ' || *line_ptr == '\t' || *line_ptr == '\n') {
        line_ptr++;
      }

      if (*line_ptr == '\0') {
        break;
      }

      char* endptr = NULL;
      double value = strtod(line_ptr, &endptr);

      if (endptr == line_ptr) {
        (void)fprintf(
            stderr, "Invalid character in file %s: '%c'\n", filename, *line_ptr
        );
        free(matrix);
        (void)fclose(file);
        return NULL;
      }

      matrix[count] = value;
      count++;

      line_ptr = endptr;
    }
  }

  (void)fclose(file);
  return matrix;
}

int write_matrix(const char* filename, const double* matrix, size_t size) {
  FILE* file = fopen(filename, "w");
  if (file == NULL) {
    perror("Failed to open output file");
    return -1;
  }

  (void)fprintf(file, "%zu\n", size);

  for (int i = 0; i < size; i++) {
    for (int j = 0; j < size; j++) {
      (void)fprintf(file, "%.6f", matrix[i * size + j]);
      if (j < size - 1) {
        (void)fprintf(file, " ");
      }
    }
    (void)fprintf(file, "\n");
  }

  (void)fclose(file);
  return 0;
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
    int argc, char* argv[], size_t* size, file_args_t* files, long* iterations
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
    } else if (strcmp(key, "--matrix1") == 0) {
      *(files->matrix1) = value;
    } else if (strcmp(key, "--matrix2") == 0) {
      *(files->matrix2) = value;
    } else if (strcmp(key, "--output") == 0) {
      *(files->result) = value;
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

void* thread_work(void* arg) {
  thread_args_t* data = (thread_args_t*)arg;
  double* matrix1 = load_matrix(data->matrix1_file, data->size);
  if (matrix1 == NULL) {
    pthread_exit(NULL);
  }

  double* matrix2 = load_matrix(data->matrix2_file, data->size);
  if (matrix2 == NULL) {
    free(matrix1);
    pthread_exit(NULL);
  }

  double* result = (double*)malloc(data->size * data->size * sizeof(double));
  if (result == NULL) {
    perror("malloc failed");
    free(matrix1);
    free(matrix2);
    pthread_exit(NULL);
  }

  for (int iter = 0; iter < data->iterations; iter++) {
    multiply_matrices(matrix1, matrix2, result, data->size);
  }

  if (write_matrix(data->output, result, data->size) != 0) {
    free(matrix1);
    free(matrix2);
    free(result);
    pthread_exit(NULL);
  }

  free(matrix1);
  free(matrix2);
  free(result);

  pthread_exit(NULL);
}

int main(int argc, char* argv[]) {
  size_t size = 0;
  char* matrix1_file = NULL;
  char* matrix2_file = NULL;
  char* output = DEFAULT_OUTPUT_FILENAME;
  file_args_t files = {&matrix1_file, &matrix2_file, &output};
  long iterations = 1;

  if (argc <= 1 ||
      parse_arguments(argc, argv, &size, &files, &iterations) != 0) {
    (void)fprintf(
        stderr,
        "Usage: %s --size <n> --matrix1 <path> --matrix2 <path> [--output "
        "<path>] "
        "[--iterations <count>]\n",
        argv[0]
    );
    return 1;
  }

  long num_threads = sysconf(_SC_NPROCESSORS_ONLN);
  pthread_t* threads = malloc(num_threads * sizeof(pthread_t));
  thread_args_t* thread_args = malloc(num_threads * sizeof(thread_args_t));

  if (threads == NULL || thread_args == NULL) {
    perror("malloc failed");
    return 1;
  }

  for (int i = 0; i < num_threads; i++) {
    thread_args[i].size = size;
    thread_args[i].matrix1_file = matrix1_file;
    thread_args[i].matrix2_file = matrix2_file;
    thread_args[i].output = output;
    thread_args[i].iterations = iterations;

    if (pthread_create(&threads[i], NULL, thread_work, &thread_args[i]) != 0) {
      perror("pthread_create failed");
      free(threads);
      free(thread_args);
      return 1;
    }
  }

  for (int i = 0; i < num_threads; i++) {
    pthread_join(threads[i], NULL);
  }

  free(threads);
  free(thread_args);
  return 0;
}
