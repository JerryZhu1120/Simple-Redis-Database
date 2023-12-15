#include "hash_map.h"

#include <stdio.h>

void hash_table_init(const char *filename, BufferPool *pool, off_t n_directory_blocks) {
    init_buffer_pool(filename, pool);
    /* TODO: add code here */
    if (pool->file.length == 0){
        Page page = EMPTY_PAGE;
        HashMapControlBlock *ctrl = (HashMapControlBlock *)&page;
        HashMapDirectoryBlock *dir = (HashMapDirectoryBlock *)&page;
        ctrl->n_directory_blocks = n_directory_blocks;
        ctrl->max_size = n_directory_blocks * HASH_MAP_DIR_BLOCK_SIZE;
        ctrl->free_block_head = -1;
        write_page(&page, &pool->file, 0); // control_block
        // directory_block
        for (int i = 0; i < n_directory_blocks; i++){
            for (int j = 0; j < HASH_MAP_DIR_BLOCK_SIZE; j++)
                dir->directory[j] = -1;
            write_page(&page, &pool->file, PAGE_SIZE * (i + 1));
        }
    }
}

void hash_table_close(BufferPool *pool) {
    close_buffer_pool(pool);
}

void hash_table_insert(BufferPool *pool, short size, off_t addr) {
    HashMapControlBlock *ctrl = (HashMapControlBlock *)get_page(pool, 0);
    HashMapDirectoryBlock *dir = (HashMapDirectoryBlock *)get_page(pool, (size / HASH_MAP_DIR_BLOCK_SIZE + 1) * PAGE_SIZE);
    HashMapBlock *block, *free_block;
    off_t block_addr = dir->directory[size % HASH_MAP_DIR_BLOCK_SIZE], temp;
    int count = 0, flag = 0;
    release(pool, (size / HASH_MAP_DIR_BLOCK_SIZE + 1) * PAGE_SIZE);
    if (block_addr == -1){
        if (ctrl->free_block_head == -1){
            dir->directory[size % HASH_MAP_DIR_BLOCK_SIZE] = pool->file.length;
            Page page = EMPTY_PAGE;
            block = (HashMapBlock *)&page;
            block->n_items = 1;
            block->next = -1;
            block->table[0] = addr;
            for (int i = 1; i < HASH_MAP_BLOCK_SIZE; i++)
                block->table[i] = -1;
            write_page(&page, &pool->file, pool->file.length);
        }
        else{
            dir->directory[size % HASH_MAP_DIR_BLOCK_SIZE] = ctrl->free_block_head;
            temp = ctrl->free_block_head;
            free_block = (HashMapBlock *)get_page(pool, ctrl->free_block_head);
            ctrl->free_block_head = free_block->next;
            free_block->n_items = 1;
            free_block->next = -1;
            free_block->table[0] = addr;
            release(pool, temp);
        }
    }
    else {
        block = (HashMapBlock*)get_page(pool, block_addr);
        while (block->n_items == HASH_MAP_BLOCK_SIZE) {
            count++;
            if (count != 1)
                release(pool, temp);
            else
                flag = 1;
            if (block->next == -1) {
                if (ctrl->free_block_head == -1) {
                    block->next = pool->file.length;
                    Page page = EMPTY_PAGE;
                    block = (HashMapBlock*)&page;
                    block->n_items = 1;
                    block->next = -1;
                    block->table[0] = addr;
                    for (int i = 1; i < HASH_MAP_BLOCK_SIZE; i++)
                        block->table[i] = -1;
                    write_page(&page, &pool->file, pool->file.length);
                }
                else {
                    block->next = ctrl->free_block_head;
                    free_block = (HashMapBlock*)get_page(pool, ctrl->free_block_head);
                    temp = ctrl->free_block_head;
                    ctrl->free_block_head = free_block->next;
                    free_block->n_items = 1;
                    free_block->next = -1;
                    free_block->table[0] = addr;
                    release(pool, temp);
                }
                release(pool, block_addr);
                release(pool, 0);
                return;
            }
            temp = block->next;
            block = (HashMapBlock*)get_page(pool, block->next);
        }
        block->table[block->n_items] = addr;
        block->n_items++;
        if (flag == 1)
            release(pool, temp);
        release(pool, block_addr);
    }
    release(pool, 0);
}

off_t hash_table_pop_lower_bound(BufferPool *pool, short size) {
    HashMapControlBlock *ctrl = (HashMapControlBlock *)get_page(pool, 0);
    HashMapDirectoryBlock *dir;
    HashMapBlock *block;
    off_t block_addr, ans;
    for (int i = 0; i < ctrl->n_directory_blocks - size / HASH_MAP_DIR_BLOCK_SIZE; i++){
        if (i == 0) {
            for (int j = 0; j < HASH_MAP_DIR_BLOCK_SIZE - size % HASH_MAP_DIR_BLOCK_SIZE; j++) {
                dir = (HashMapDirectoryBlock*)get_page(pool, (size / HASH_MAP_DIR_BLOCK_SIZE + 1 + i) * PAGE_SIZE);
                block_addr = dir->directory[size % HASH_MAP_DIR_BLOCK_SIZE + j];
                if (block_addr != -1) {
                    block = (HashMapBlock*)get_page(pool, block_addr);
                    ans = block->table[block->n_items - 1];
                    block->n_items--;
                    block->table[block->n_items] = -1;
                    if (block->n_items == 0) {
                        dir->directory[size % HASH_MAP_DIR_BLOCK_SIZE + j] = block->next;
                        block->next = ctrl->free_block_head;
                        ctrl->free_block_head = block_addr;
                    }
                    release(pool, block_addr);
                    release(pool, (size / HASH_MAP_DIR_BLOCK_SIZE + 1 + i) * PAGE_SIZE);
                    release(pool, 0);
                    return ans;
                }
                release(pool, (size / HASH_MAP_DIR_BLOCK_SIZE + 1 + i) * PAGE_SIZE);
            }
        }
        else {
            for (int j = 0; j < HASH_MAP_DIR_BLOCK_SIZE; j++) {
                dir = (HashMapDirectoryBlock*)get_page(pool, (size / HASH_MAP_DIR_BLOCK_SIZE + 1 + i) * PAGE_SIZE);
                block_addr = dir->directory[j];
                if (block_addr != -1) {
                    block = (HashMapBlock*)get_page(pool, block_addr);
                    ans = block->table[block->n_items - 1];
                    block->n_items--;
                    block->table[block->n_items] = -1;
                    if (block->n_items == 0) {
                        dir->directory[j] = block->next;
                        block->next = ctrl->free_block_head;
                        ctrl->free_block_head = block_addr;
                    }
                    release(pool, block_addr);
                    release(pool, (size / HASH_MAP_DIR_BLOCK_SIZE + 1 + i) * PAGE_SIZE);
                    release(pool, 0);
                    return ans;
                }
                release(pool, (size / HASH_MAP_DIR_BLOCK_SIZE + 1 + i) * PAGE_SIZE);
            }
        }
    }
    release(pool, 0);
    return -1;
}

void hash_table_pop(BufferPool *pool, short size, off_t addr) {
    HashMapControlBlock *ctrl = (HashMapControlBlock *)get_page(pool, 0);
    HashMapDirectoryBlock *dir;
    HashMapBlock *block, *next_block;
    off_t block_addr, temp;
    dir = (HashMapDirectoryBlock *)get_page(pool, (size / HASH_MAP_DIR_BLOCK_SIZE + 1) * PAGE_SIZE);
    block_addr = dir->directory[size % HASH_MAP_DIR_BLOCK_SIZE];
    block = (HashMapBlock *)get_page(pool, block_addr);
    for (int i = 0; i < block->n_items; i++){
        if (block->table[i] == addr){
            for (int j = i; j < block->n_items - 1; j++)
                block->table[j] = block->table[j + 1];
            block->table[block->n_items - 1] = -1;
            block->n_items--;
            if (block->n_items == 0){
                dir->directory[size % HASH_MAP_DIR_BLOCK_SIZE] = block->next;
                block->next = ctrl->free_block_head;
                ctrl->free_block_head = block_addr;
            }
            release(pool, block_addr);
            release(pool, (size / HASH_MAP_DIR_BLOCK_SIZE + 1) * PAGE_SIZE);
            release(pool, 0);
            return;
        }
    }
    while (block->next != -1){
        next_block = (HashMapBlock *)get_page(pool, block->next);
        temp = block->next;
        for (int i = 0; i < next_block->n_items; i++){
            if (next_block->table[i] == addr){
                for (int j = i; j < next_block->n_items - 1; j++)
                    next_block->table[j] = next_block->table[j + 1];
                next_block->table[next_block->n_items - 1] = -1;
                next_block->n_items--;
                if (next_block->n_items == 0){
                    block->next = next_block->next;
                    next_block->next = ctrl->free_block_head;
                    ctrl->free_block_head = temp;
                }
                release(pool, temp);
                release(pool, block_addr);
                release(pool, (size / HASH_MAP_DIR_BLOCK_SIZE + 1) * PAGE_SIZE);
                release(pool, 0);
                return;
            }
        }
        block = next_block;
        release(pool, temp);
    }
    release(pool, block_addr);
    release(pool, (size / HASH_MAP_DIR_BLOCK_SIZE + 1) * PAGE_SIZE);
    release(pool, 0);
}

void print_hash_table(BufferPool *pool) {
    HashMapControlBlock *ctrl = (HashMapControlBlock*)get_page(pool, 0);
    HashMapDirectoryBlock *dir_block;
    off_t block_addr, next_addr;
    HashMapBlock *block;
    int i, j;
    printf("----------HASH TABLE----------\n");
    for (i = 0; i < ctrl->max_size; ++i) {
        dir_block = (HashMapDirectoryBlock*)get_page(pool, (i / HASH_MAP_DIR_BLOCK_SIZE + 1) * PAGE_SIZE);
        if (dir_block->directory[i % HASH_MAP_DIR_BLOCK_SIZE] != -1) {
            printf("%d:", i);
            block_addr = dir_block->directory[i % HASH_MAP_DIR_BLOCK_SIZE];
            while (block_addr != -1) {
                block = (HashMapBlock*)get_page(pool, block_addr);
                printf("  [" FORMAT_OFF_T "]", block_addr);
                printf("{");
                for (j = 0; j < block->n_items; ++j) {
                    if (j != 0) {
                        printf(", ");
                    }
                    printf(FORMAT_OFF_T, block->table[j]);
                }
                printf("}");
                next_addr = block->next;
                release(pool, block_addr);
                block_addr = next_addr;
            }
            printf("\n");
        }
        release(pool, (i / HASH_MAP_DIR_BLOCK_SIZE + 1) * PAGE_SIZE);
    }
    release(pool, 0);
    printf("------------------------------\n");
}