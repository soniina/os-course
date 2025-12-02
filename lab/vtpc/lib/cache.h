#pragma once

#include <sys/types.h>
#include <stdint.h>

#define PAGE_SIZE 4096

typedef struct cache_page_t {
    int fd;                
    off_t page_index;     
    char *data;            
    int is_dirty;     
    int is_free;    
    struct cache_page_t *prev;
    struct cache_page_t *next;
    struct cache_page_t *hash_next; 
} cache_page_t;

typedef struct {
    cache_page_t *head;    
    cache_page_t *tail; 
    cache_page_t **hash_table;
    size_t capacity;    
    size_t hash_size;  
    size_t size;        
    cache_page_t *pages_pool; 
} cache_t;

int cache_init(size_t capacity);
int cache_destroy();
cache_page_t* cache_get_page(int fd, off_t page_index);
void cache_mark_dirty(cache_page_t *page);
int cache_sync_file(int fd);
