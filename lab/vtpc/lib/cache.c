#define _GNU_SOURCE
#include "cache.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static cache_t cache;

static unsigned int get_hash_index(int fd, off_t page_index) {
  return (unsigned int)((fd + page_index) % cache.hash_size);
}

static void lru_unlink(cache_page_t* page) {
  if (page->prev) {
    page->prev->next = page->next;
  }
  if (page->next) {
    page->next->prev = page->prev;
  }

  if (page == cache.head) {
    cache.head = page->next;
  }
  if (page == cache.tail) {
    cache.tail = page->prev;
  }

  page->prev = NULL;
  page->next = NULL;
}

static void lru_push_head(cache_page_t* page) {
  page->next = cache.head;
  page->prev = NULL;

  if (cache.head) {
    cache.head->prev = page;
  }
  cache.head = page;

  if (!cache.tail) {
    cache.tail = page;
  }
}

static void hash_remove(cache_page_t* page) {
  unsigned int h = get_hash_index(page->fd, page->page_index);
  cache_page_t* curr = cache.hash_table[h];
  cache_page_t* prev = NULL;

  while (curr) {
    if (curr == page) {
      if (prev) {
        prev->hash_next = curr->hash_next;
      } else {
        cache.hash_table[h] = curr->hash_next;
      }
      return;
    }
    prev = curr;
    curr = curr->hash_next;
  }
}

static void hash_insert(cache_page_t* page) {
  unsigned int h = get_hash_index(page->fd, page->page_index);
  page->hash_next = cache.hash_table[h];
  cache.hash_table[h] = page;
}

static int flush_page(cache_page_t* page) {
  if (page->is_free || !page->is_dirty) {
    return 0;
  }

  struct stat st;
  if (fstat(page->fd, &st) == -1) {
    perror("fstat failed");
    st.st_size = -1;
  }

  off_t offset = page->page_index * PAGE_SIZE;

  ssize_t res = pwrite(page->fd, page->data, PAGE_SIZE, offset);
  if (res < 0) {
    perror("pwrite failed");
    return -1;
  }

  if (st.st_size != -1) {
    if (offset < st.st_size && (offset + PAGE_SIZE) > st.st_size) {
      if (ftruncate(page->fd, st.st_size) == -1) {
        perror("flush ftruncate fix failed");
      }
    }
  }
  page->is_dirty = 0;
  return 0;
}

int cache_init(size_t capacity) {
  cache.capacity = capacity;
  cache.hash_size = capacity * 2;
  cache.head = NULL;
  cache.tail = NULL;
  cache.size = 0;

  cache.pages_pool =
      (cache_page_t*)calloc(cache.capacity, sizeof(cache_page_t));
  if (!cache.pages_pool) {
    perror("calloc failed");
    return -1;
  }

  cache.hash_table = calloc(cache.hash_size, sizeof(cache_page_t*));
  if (!cache.hash_table) {
    perror("calloc failed");
    free(cache.pages_pool);
    return -1;
  }

  for (int i = 0; i < cache.capacity; i++) {
    if (posix_memalign(
            (void**)&cache.pages_pool[i].data, PAGE_SIZE, PAGE_SIZE
        ) != 0) {
      perror("posix_memalign failed");
      return -1;
    }

    cache.pages_pool[i].is_free = 1;
    cache.pages_pool[i].is_dirty = 0;
  }

  return 0;
}

cache_page_t* cache_get_page(int fd, off_t page_index) {
  unsigned int h = get_hash_index(fd, page_index);
  cache_page_t* curr = cache.hash_table[h];
  while (curr) {
    if (!curr->is_free && curr->fd == fd && curr->page_index == page_index) {
      lru_unlink(curr);
      lru_push_head(curr);
      return curr;
    }
    curr = curr->hash_next;
  }

  cache_page_t* page = NULL;
  if (cache.size < cache.capacity) {
    for (int i = 0; i < cache.capacity; i++) {
      if (cache.pages_pool[i].is_free) {
        page = &cache.pages_pool[i];
        cache.size++;
        break;
      }
    }
  } else {
    page = cache.tail;
    flush_page(page);
    hash_remove(page);
    lru_unlink(page);
  }

  off_t offset = page_index * PAGE_SIZE;
  ssize_t res = pread(fd, page->data, PAGE_SIZE, offset);
  if (res < 0) {
    perror("pread failed");
    return NULL;
  }
  if (res < PAGE_SIZE) {
    memset(page->data + res, 0, PAGE_SIZE - res);
  }

  page->fd = fd;
  page->page_index = page_index;
  page->is_free = 0;
  page->is_dirty = 0;

  hash_insert(page);
  lru_push_head(page);

  return page;
}

int cache_destroy() {
  if (!cache.pages_pool) {
    return 0;
  }

  for (int i = 0; i < cache.capacity; i++) {
    if (!cache.pages_pool[i].is_free) {
      flush_page(&cache.pages_pool[i]);
    }
    free(cache.pages_pool[i].data);
  }

  free(cache.pages_pool);
  cache.pages_pool = NULL;

  free(cache.hash_table);
  cache.hash_table = NULL;

  cache.head = NULL;
  cache.tail = NULL;
  cache.size = 0;

  return 0;
}

void cache_mark_dirty(cache_page_t* page) {
  if (page) {
    page->is_dirty = 1;
    lru_unlink(page);
    lru_push_head(page);
  }
}

int cache_sync_file(int fd) {
  int res = 0;
  for (int i = 0; i < cache.capacity; i++) {
    if (!cache.pages_pool[i].is_free && cache.pages_pool[i].fd == fd) {
      if (flush_page(&cache.pages_pool[i]) != 0) {
        res = -1;
      }
    }
  }
  if (fsync(fd) < 0) {
    perror("fsync failed");
    res = -1;
  }
  return res;
}
