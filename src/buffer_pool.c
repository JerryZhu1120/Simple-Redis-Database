#include "buffer_pool.h"
#include "file_io.h"

#include <stdio.h>
#include <stdlib.h>

void init_buffer_pool(const char *filename, BufferPool *pool) {
    open_file(&pool->file, filename);
    for (int i = 0; i < CACHE_PAGE; i++){
        pool->addrs[i] = -1;
        pool->lru[i] = -1;
        pool->ref[i] = 0;
    }
}

void close_buffer_pool(BufferPool *pool) {
    for (int i = 0; i < CACHE_PAGE; i++)
        write_page(&pool->pages[i], &pool->file, pool->addrs[i]);
    close_file(&pool->file);
}

Page *get_page(BufferPool *pool, off_t addr) {
    size_t temp;
    for (int i = 0; i < CACHE_PAGE; i++){
        if (pool->addrs[i] == addr){
            pool->ref[i]++;
            for (int j = 0; j < CACHE_PAGE; j++){
                if (pool->lru[j] == addr){
                    temp = pool->lru[j];
                    for (int k = j; k >= 1; k--)
                        pool->lru[k] = pool->lru[k - 1];
                    pool->lru[0] = temp;
                    break;
                }
            }
            return &pool->pages[i];
        }
    }
    for (int i = 0; i < CACHE_PAGE; i++){
        if (pool->addrs[i] == -1){
            read_page(&pool->pages[i], &pool->file, addr);
            pool->addrs[i] = addr;
            pool->ref[i]++;
            for (int j = CACHE_PAGE - 2; j >= 0; j--)
                pool->lru[j + 1] = pool->lru[j];
            pool->lru[0] = addr;
            return &pool->pages[i];
        }
    }
    for (int i = CACHE_PAGE - 1; i >= 0; i--){
        for (int j = 0; j < CACHE_PAGE; j++){
            if (pool->addrs[j] == pool->lru[i]){
                if (pool->ref[j] == 0){
                    write_page(&pool->pages[j], &pool->file, pool->addrs[j]);
                    read_page(&pool->pages[j], &pool->file, addr);
                    pool->addrs[j] = addr;
                    pool->ref[j]++;
                    for (int k = i - 1; k >= 0; k--)
                        pool->lru[k + 1] = pool->lru[k];
                    pool->lru[0] = addr;
                    return &pool->pages[j];
                }
            }
        }
    }
}

void release(BufferPool *pool, off_t addr) {
    for (int i = 0; i < CACHE_PAGE; i++){
        if (pool->addrs[i] == addr){
            pool->ref[i]--;
            break;
        }
    }
}

/* void print_buffer_pool(BufferPool *pool) {
} */

/* void validate_buffer_pool(BufferPool *pool) {
} */
