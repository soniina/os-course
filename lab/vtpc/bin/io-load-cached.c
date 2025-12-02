#define _GNU_SOURCE

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include "vtpc.h"

#define BASE_DECIMAL 10
#define DEFAULT_RANGE_START 0
#define DEFAULT_RANGE_END 0
#define FILE_MODE 0644

typedef enum { MODE_READ, MODE_WRITE } rw_mode_t;

typedef enum { ACCESS_SEQUENTIAL, ACCESS_RANDOM } access_type_t;

typedef struct {
  rw_mode_t rw_mode;
  size_t block_size;
  size_t block_count;
  const char* filename;
  off_t range_start;
  off_t range_end;
  access_type_t access_type;
  size_t cache_limit;
} io_params_t;

typedef struct {
  off_t start;
  off_t end;
} io_range_t;

static int parse_range(const char* range_str, off_t* start, off_t* end) {
  char* dash = strchr(range_str, '-');
  if (!dash) {
    (void)fprintf(stderr, "Invalid range format: %s\n", range_str);
    return -1;
  }
  *dash = '\0';
  char* endptr1 = NULL;
  char* endptr2 = NULL;
  *start = strtoll(range_str, &endptr1, BASE_DECIMAL);
  *end = strtoll(dash + 1, &endptr2, BASE_DECIMAL);
  if (*endptr1 != '\0' || *endptr2 != '\0' || *start < 0 || *end < *start) {
    (void)fprintf(stderr, "Invalid range values: %s\n", range_str);
    return -1;
  }
  return 0;
}

static int parse_rw_mode(const char* value, rw_mode_t* mode) {
  if (strcmp(value, "read") == 0) {
    *mode = MODE_READ;
  } else if (strcmp(value, "write") == 0) {
    *mode = MODE_WRITE;
  } else {
    (void)fprintf(
        stderr, "Invalid rw mode: %s (must be 'read' or 'write')\n", value
    );
    return -1;
  }
  return 0;
}

static int parse_access_type(const char* value, access_type_t* type) {
  if (strcmp(value, "sequence") == 0) {
    *type = ACCESS_SEQUENTIAL;
  } else if (strcmp(value, "random") == 0) {
    *type = ACCESS_RANDOM;
  } else {
    (void)fprintf(
        stderr,
        "Invalid access type: %s (must be 'sequence' or 'random')\n",
        value
    );
    return -1;
  }
  return 0;
}

static int parse_size_param(
    const char* value, const char* param_name, size_t* result
) {
  char* endptr = NULL;
  long size = strtol(value, &endptr, BASE_DECIMAL);
  if (*endptr != '\0' || size <= 0) {
    (void)fprintf(stderr, "Invalid %s: %s\n", param_name, value);
    return -1;
  }
  *result = (size_t)size;
  return 0;
}

static int parse_single_argument(
    const char* key, io_params_t* params, const char* value
) {
  if (strcmp(key, "--rw") == 0) {
    if (parse_rw_mode(value, &params->rw_mode) != 0) {
      return -1;
    }
  } else if (strcmp(key, "--block_size") == 0) {
    if (parse_size_param(value, "block_size", &params->block_size) != 0) {
      return -1;
    }
  } else if (strcmp(key, "--block_count") == 0) {
    if (parse_size_param(value, "block_count", &params->block_count) != 0) {
      return -1;
    }
  } else if (strcmp(key, "--cache_limit") == 0) {
    if (parse_size_param(value, "cache_limit", &params->cache_limit) != 0) {
      return -1;
    }
  } else if (strcmp(key, "--file") == 0) {
    params->filename = value;
  } else if (strcmp(key, "--range") == 0) {
    if (parse_range(value, &params->range_start, &params->range_end) != 0) {
      return -1;
    }
  } else if (strcmp(key, "--type") == 0) {
    if (parse_access_type(value, &params->access_type) != 0) {
      return -1;
    }
  } else {
    (void)fprintf(stderr, "Unknown parameter: %s\n", key);
    return -1;
  }
  return 0;
}

static int parse_arguments(
    int argc, char* argv[], io_params_t* params, const long page_size
) {
  params->rw_mode = MODE_READ;
  params->block_size = 0;
  params->block_count = 0;
  params->filename = NULL;
  params->range_start = DEFAULT_RANGE_START;
  params->range_end = DEFAULT_RANGE_END;
  params->access_type = ACCESS_SEQUENTIAL;
  params->cache_limit = 0;

  for (int i = 1; i < argc; i++) {
    char* key = argv[i++];
    if (i >= argc) {
      (void)fprintf(stderr, "Missing value for --%s\n", key);
      return -1;
    }
    char* value = argv[i];

    if (parse_single_argument(key, params, value) != 0) {
      return -1;
    }
  }

  if (params->block_size % page_size != 0) {
    (void)fprintf(
        stderr, "For O_DIRECT, block_size must be multiple of %ld\n", page_size
    );
    return -1;
  }

  if (params->block_size == 0 || params->block_count == 0 ||
      params->filename == NULL) {
    (void)fprintf(stderr, "Missing required parameters\n");
    return -1;
  }
  return 0;
}

static int determine_working_range(
    io_params_t* params, int file_descriptor, io_range_t* actual_range
) {
  off_t file_size = vtpc_lseek(file_descriptor, 0, SEEK_END);
  if (file_size == -1) {
    perror("vtpc_lseek failed");
    return -1;
  }

  if (params->range_start == DEFAULT_RANGE_START &&
      params->range_end == DEFAULT_RANGE_END) {
    actual_range->start = 0;
    if (params->rw_mode == MODE_READ) {
      actual_range->end = file_size;
    } else {
      actual_range->end = actual_range->start + (off_t)params->block_size;
    }
  }

  if (params->rw_mode == MODE_READ) {
    if (actual_range->end > file_size) {
      (void)fprintf(
          stderr,
          "Range end (%lld) exceeds file size (%lld)\n",
          (long long)actual_range->end,
          (long long)file_size
      );
      return -1;
    }
  } else {
    if (actual_range->end > file_size) {
      if (ftruncate(file_descriptor, actual_range->end) == -1) {
        perror("ftruncate failed");
        return -1;
      }
    }
  }

  if ((actual_range->end - actual_range->start) < (off_t)params->block_size) {
    (void)fprintf(stderr, "Range too small for block size\n");
    return -1;
  }

  return 0;
}

static int perform_io_operation(
    int file_descriptor, void* buffer, const io_params_t* params, off_t position
) {
  if (vtpc_lseek(file_descriptor, position, SEEK_SET) == -1) {
    perror("vtpc_lseek failed");
    return -1;
  }

  ssize_t result = 0;
  if (params->rw_mode == MODE_READ) {
    result = vtpc_read(file_descriptor, buffer, params->block_size);
  } else {
    result = vtpc_write(file_descriptor, buffer, params->block_size);
  }

  if (result != (ssize_t)params->block_size) {
    perror(params->rw_mode == MODE_READ ? "read failed" : "write failed");
    return -1;
  }

  return 0;
}

static int sequential_io(
    int file_descriptor,
    void* buffer,
    const io_params_t* params,
    const io_range_t* range
) {
  off_t current_position = range->start;
  const off_t max_position = range->end - (off_t)params->block_size;

  for (size_t i = 0; i < params->block_count; i++) {
    if (perform_io_operation(
            file_descriptor, buffer, params, current_position
        ) != 0) {
      return 1;
    }

    current_position += (off_t)params->block_size;
    if (current_position > max_position) {
      current_position = range->start;
    }
  }

  return 0;
}

static int random_io(
    int file_descriptor,
    void* buffer,
    const io_params_t* params,
    const io_range_t* range
) {
  const off_t max_offset = range->end - (off_t)params->block_size;
  const off_t range_size = max_offset - range->start + 1;

  for (size_t i = 0; i < params->block_count; i++) {
    const off_t random_offset = (random() % range_size);
    const off_t position = range->start + random_offset;

    if (perform_io_operation(file_descriptor, buffer, params, position) != 0) {
      return -1;
    }
  }

  return 0;
}

int main(int argc, char* argv[]) {
  const long page_size = sysconf(_SC_PAGESIZE);
  if (page_size == -1) {
    perror("sysconf failed");
    return 1;
  }
  io_params_t params;

  if (parse_arguments(argc, argv, &params, page_size) != 0) {
    (void)fprintf(
        stderr,
        "Usage: %s --rw <read|write> --block_size <size> --block_count "
        "<count> --cache_limit <limit>"
        "--file <path> [--range <start-end>] [--type "
        "<sequence|random>]\n",
        argv[0]
    );
    return 1;
  }

  vtpc_set_capacity(params.cache_limit);
  int flags = params.rw_mode == MODE_READ
                  ? O_RDONLY
                  : (unsigned int)O_WRONLY | (unsigned int)O_CREAT;
  flags |= O_DIRECT;

  int file_descriptor = vtpc_open(params.filename, flags, FILE_MODE);
  if (file_descriptor == -1) {
    perror("vtpc_open failed");
    return 1;
  }

  io_range_t actual_range = {params.range_start, params.range_end};
  if (determine_working_range(&params, file_descriptor, &actual_range) != 0) {
    vtpc_close(file_descriptor);
    return 1;
  }

  void* buffer = NULL;
  if (posix_memalign(&buffer, page_size, params.block_size) != 0) {
    perror("posix_memalign failed");
  }
  if (buffer == NULL) {
    vtpc_close(file_descriptor);
    return 1;
  }

  if (params.rw_mode == MODE_WRITE) {
    memset(buffer, 0xAA, params.block_size);
  }

  int result = 0;
  if (params.access_type == ACCESS_RANDOM) {
    srandom((unsigned int)time(NULL));
    result = random_io(file_descriptor, buffer, &params, &actual_range);
  } else {
    result = sequential_io(file_descriptor, buffer, &params, &actual_range);
  }

  free(buffer);
  vtpc_close(file_descriptor);

  return result != 0 ? 1 : 0;
}
