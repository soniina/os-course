#define _GNU_SOURCE

#include "vtpc.h"

#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "cache.h"

static int cache_is_initialized = 0;

static size_t g_cache_capacity = DEFAULT_CACHE_CAPACITY;

void vtpc_set_capacity(size_t capacity) {
  if (capacity > 0) {
    g_cache_capacity = capacity;
  }
}

int vtpc_open(const char* path, int mode, int access) {
  if (!cache_is_initialized) {
    if (cache_init(g_cache_capacity) != 0) {
      perror("cache_init failed");
      return -1;
    }
    cache_is_initialized = 1;
  }
  int fd = open(path, mode | O_DIRECT, access);
  if (fd < 0) {
    perror("open failed");
    return -1;
  }
  return fd;
}

int vtpc_close(int fd) {
  cache_sync_file(fd);
  return close(fd);
}

static off_t get_file_size(int fd) {
  struct stat st;
  if (fstat(fd, &st) == -1) {
    perror("fstat failed");
    st.st_size = -1;
  }
  return st.st_size;
}

ssize_t vtpc_read(int fd, void* buf, size_t count) {
  off_t file_offset = lseek(fd, 0, SEEK_CUR);
  if (file_offset == -1) {
    perror("lseek (cur) failed");
    return -1;
  }

  off_t file_size = get_file_size(fd);
  if (file_size == -1) {
    return -1;
  }

  if (file_offset >= file_size) {
    return 0;
  }

  size_t bytes_available = file_size - file_offset;
  if (count > bytes_available) {
    count = bytes_available;
  }
  size_t total_read = 0;
  char* ptr = (char*)buf;

  while (total_read < count) {
    off_t current_pos = file_offset + (off_t)total_read;
    off_t page_index = current_pos / PAGE_SIZE;
    off_t page_off = current_pos % PAGE_SIZE;

    size_t to_copy = count - total_read;
    if (PAGE_SIZE - page_off < to_copy) {
      to_copy = PAGE_SIZE - page_off;
    }

    cache_page_t* page = cache_get_page(fd, page_index);
    if (!page) {
      return (total_read > 0) ? (ssize_t)total_read : -1;
    }
    memcpy(ptr + total_read, page->data + page_off, to_copy);

    total_read += to_copy;
  }

  if (lseek(fd, (off_t)total_read, SEEK_CUR) == -1) {
    perror("seek (cur) failed");
    return -1;
  }
  return (ssize_t)total_read;
}

ssize_t vtpc_write(int fd, const void* buf, size_t count) {
  off_t file_offset = lseek(fd, 0, SEEK_CUR);
  if (file_offset == -1) {
    perror("lseek (cur) failed");
    return -1;
  }

  off_t file_size = get_file_size(fd);
  if (file_size == -1) {
    return -1;
  }

  off_t write_end_pos = file_offset + (off_t)count;
  if (write_end_pos > file_size) {
    if (ftruncate(fd, write_end_pos) == -1) {
      perror("ftruncate failed");
      return -1;
    }
  }

  size_t total_write = 0;
  const char* ptr = (const char*)buf;

  while (total_write < count) {
    off_t current_pos = file_offset + (off_t)total_write;
    off_t page_index = current_pos / PAGE_SIZE;
    off_t page_off = current_pos % PAGE_SIZE;

    size_t to_copy = count - total_write;
    if (PAGE_SIZE - page_off < to_copy) {
      to_copy = PAGE_SIZE - page_off;
    }

    cache_page_t* page = cache_get_page(fd, page_index);
    if (!page) {
      break;
    }
    memcpy(page->data + page_off, ptr + total_write, to_copy);
    cache_mark_dirty(page);

    total_write += to_copy;
  }

  if (lseek(fd, (off_t)total_write, SEEK_CUR) == -1) {
    perror("lseek failed (cur)");
    return -1;
  }

  if (total_write < count && total_write == 0) {
    return -1;
  }

  return (ssize_t)total_write;
}

off_t vtpc_lseek(int fd, off_t offset, int whence) {
  return lseek(fd, offset, whence);
}

int vtpc_fsync(int fd) {
  return cache_sync_file(fd);
}
