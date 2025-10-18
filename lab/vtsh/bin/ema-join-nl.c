#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define BASE_DECIMAL 10
#define WORD_LENGTH 9
#define DEFAULT_OUPUT_FILENAME "result.txt"
#define BUFFER_SIZE 32

typedef struct {
  int id;
  char word[WORD_LENGTH];
} row_t;

row_t* load_table(const char* filename, int* count) {
  FILE* file = fopen(filename, "r");
  if (file == NULL) {
    perror(filename);
    return NULL;
  }

  char buffer[BUFFER_SIZE];
  if (fgets(buffer, sizeof(buffer), file) == NULL) {
    perror("fgets failed");
    (void)fclose(file);
    return NULL;
  }

  char* endptr = 0;
  long num_rows = strtol(buffer, &endptr, BASE_DECIMAL);
  if (endptr == buffer || num_rows <= 0) {
    (void)fprintf(stderr, "Invalid number of rows in file: %s\n", filename);
    (void)fclose(file);
    return NULL;
  }
  *count = (int)num_rows;

  row_t* table = malloc(*count * sizeof(row_t));
  if (table == NULL) {
    perror("malloc failed");
    (void)fclose(file);
    return NULL;
  }

  for (int i = 0; i < *count; i++) {
    if (fgets(buffer, sizeof(buffer), file) == NULL) {
      perror("fgets failed");
      free(table);
      (void)fclose(file);
      return NULL;
    }

    char* saveptr = NULL;
    char* token1 = strtok_r(buffer, " \t\n", &saveptr);
    char* token2 = strtok_r(NULL, " \t\n", &saveptr);
    if (token1 == NULL || token2 == NULL) {
      (void)fprintf(
          stderr, "Invalid format in file: %s (line %d)\n", filename, i + 1
      );
      free(table);
      (void)fclose(file);
      return NULL;
    }

    table[i].id = (int)strtol(token1, &endptr, BASE_DECIMAL);
    if (endptr == token1) {
      (void
      )fprintf(stderr, "Invalid id in file: %s (line %d)\n", filename, i + 1);
      free(table);
      (void)fclose(file);
      return NULL;
    }

    strncpy(table[i].word, token2, WORD_LENGTH - 1);
    table[i].word[WORD_LENGTH - 1] = '\0';
  }

  (void)fclose(file);
  return table;
}

void nested_loop_join(
    row_t* table1, int count1, row_t* table2, int count2, FILE* output
) {
  int total_rows = 0;
  for (int i = 0; i < count1; i++) {
    for (int j = 0; j < count2; j++) {
      if (table1[i].id == table2[j].id) {
        total_rows++;
      }
    }
  }

  (void)fprintf(output, "%d\n", total_rows);
  for (int i = 0; i < count1; i++) {
    for (int j = 0; j < count2; j++) {
      if (table1[i].id == table2[j].id) {
        (void)fprintf(
            output, "%d %s %s\n", table1[i].id, table1[i].word, table2[j].word
        );
      }
    }
  }
}

int main(int argc, char* argv[]) {
  char* file1 = NULL;
  char* file2 = NULL;
  char* output = DEFAULT_OUPUT_FILENAME;
  int iterations = 1;

  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--file1") == 0) {
      file1 = argv[++i];
    } else if (strcmp(argv[i], "--file2") == 0) {
      file2 = argv[++i];
    } else if (strcmp(argv[i], "--output") == 0) {
      output = argv[++i];
    } else if (strcmp(argv[i], "--iterations") == 0) {
      char* endptr = 0;
      iterations = (int)strtol(argv[++i], &endptr, BASE_DECIMAL);
      if (*endptr != '\0') {
        (void)fprintf(stderr, "Invalid iterations: %s\n", argv[i]);
        return 1;
      }
    }
  }

  if (!file1 || !file2) {
    (void)fprintf(
        stderr,
        "Usage: %s --file1 <path> --file2 <path> [--output <path>] "
        "[--iterations <count>]\n",
        argv[0]
    );
    return 1;
  }

  for (int iter = 0; iter < iterations; iter++) {
    int count1 = 0;
    int count2 = 0;
    row_t* table1 = load_table(file1, &count1);
    if (!table1) {
      return 1;
    }
    row_t* table2 = load_table(file2, &count2);
    if (!table2) {
      free(table1);
      return 1;
    }

    FILE* file = fopen(output, "w");
    if (file == NULL) {
      perror("Failed to open output file");
      free(table1);
      free(table2);
      return 1;
    }
    nested_loop_join(table1, count1, table2, count2, file);
    (void)fclose(file);

    free(table1);
    free(table2);
  }

  return 0;
}
