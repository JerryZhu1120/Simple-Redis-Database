#include "block.h"

#include <stdio.h>

void init_block(Block *block) {
    block->n_items = 0;
    block->head_ptr = (short)(block->data - (char*)block);
    block->tail_ptr = (short)sizeof(Block);
}

ItemPtr get_item(Block *block, short idx) {
    if (idx < 0 || idx >= block->n_items) {
        printf("get item error: idx is out of range\n");
        return NULL;
    }
    ItemID item_id = get_item_id(block, idx);
    if (get_item_id_availability(item_id)) {
        printf("get item error: item_id is not used\n");
        return NULL;
    }
    short offset = get_item_id_offset(item_id);
    return (char*)block + offset;
}

short new_item(Block *block, ItemPtr item, short item_size) {
    ItemID id;
    for (int i = 0; i < block->n_items; i++){
        id = get_item_id(block, i);
        if (get_item_id_availability(id) == 1) {
            block->tail_ptr -= item_size;
            get_item_id(block, i) = compose_item_id(0, block->tail_ptr, item_size);
            for (int j = 0; j < item_size; j++)
                *((char*)block + block->tail_ptr + j) = item[j];
            return i;
        }
    }
    block->head_ptr += sizeof(ItemID);
    block->tail_ptr -= item_size;
    get_item_id(block, block->n_items) = compose_item_id(0, block->tail_ptr, item_size);
    block->n_items++;
    for (int i = 0; i < item_size; i++)
        *((char*)block + block->tail_ptr + i) = item[i];
    return block->n_items - 1;
}

void delete_item(Block *block, short idx) {
    ItemID id;
    off_t offset;
    short size;
    id = get_item_id(block, idx);
    offset = get_item_id_offset(id);
    size = get_item_id_size(id);
    if (idx == block->n_items - 1) {
        block->n_items--;
        block->head_ptr -= sizeof(ItemID);
        for (int i = 0; i < offset - block->tail_ptr; i++)
            *((char*)block + offset + size - 1 - i) = *((char*)block + offset - 1 - i);
        for (int i = 0; i < block->n_items; i++) {
            id = get_item_id(block, i);
            if (get_item_id_offset(id) < offset && get_item_id_availability(id) == 0)
                get_item_id(block, i) = compose_item_id(get_item_id_availability(id), get_item_id_offset(id) + size, get_item_id_size(id));
        }
    }
    else {
        get_item_id(block, idx) = compose_item_id(1, 0, 0);
        for (int i = 0; i < offset - block->tail_ptr; i++)
            *((char*)block + offset + size - 1 - i) = *((char*)block + offset - 1 - i);
        for (int i = 0; i < block->n_items; i++) {
            id = get_item_id(block, i);
            if (get_item_id_offset(id) < offset && get_item_id_availability(id) == 0)
                get_item_id(block, i) = compose_item_id(get_item_id_availability(id), get_item_id_offset(id) + size, get_item_id_size(id));
        }
    }
    block->tail_ptr += size;
}

short free_space(Block* block) {
     short ans = block->tail_ptr - block->head_ptr - sizeof(ItemID);
     ItemID id;
     for (int i = 0; i < block->n_items; i++) {
         id = get_item_id(block, i);
         if (get_item_id_availability(id) == 1)
             return block->tail_ptr - block->head_ptr;
     }
     if (ans < 0)
         return 0;
     else
         return ans;
}

void str_printer(ItemPtr item, short item_size) {
    if (item == NULL) {
        printf("NULL");
        return;
    }
    short i;
    printf("\"");
    for (i = 0; i < item_size; ++i) {
        printf("%c", item[i]);
    }
    printf("\"");
}

// void print_block(Block *block, printer_t printer) {
//     short i, availability, offset, size;
//     ItemID item_id;
//     ItemPtr item;
//     printf("----------BLOCK----------\n");
//     printf("total = %d\n", block->n_items);
//     printf("head = %d\n", block->head_ptr);
//     printf("tail = %d\n", block->tail_ptr);
//     for (i = 0; i < block->n_items; ++i) {
//         item_id = get_item_id(block, i);
//         availability = get_item_id_availability(item_id);
//         offset = get_item_id_offset(item_id);
//         size = get_item_id_size(item_id);
//         if (!availability) {
//             item = get_item(block, i);
//         } else {
//             item = NULL;
//         }
//         printf("%10d%5d%10d%10d\t", i, availability, offset, size);
//         printer(item, size);
//         printf("\n");
//     }
//     printf("-------------------------\n");
// }
/*
void analyze_block(Block *block, block_stat_t *stat) {
    short i;
    stat->empty_item_ids = 0;
    stat->total_item_ids = block->n_items;
    for (i = 0; i < block->n_items; ++i) {
        if (get_item_id_availability(get_item_id(block, i))) {
            ++stat->empty_item_ids;
        }
    }
    stat->available_space = block->tail_ptr - block->head_ptr 
        + stat->empty_item_ids * sizeof(ItemID);
}

void accumulate_stat_info(block_stat_t *stat, const block_stat_t *stat2) {
    stat->empty_item_ids += stat2->empty_item_ids;
    stat->total_item_ids += stat2->total_item_ids;
    stat->available_space += stat2->available_space;
}

void print_stat_info(const block_stat_t *stat) {
    printf("==========STAT==========\n");
    printf("empty_item_ids: " FORMAT_SIZE_T "\n", stat->empty_item_ids);
    printf("total_item_ids: " FORMAT_SIZE_T "\n", stat->total_item_ids);
    printf("available_space: " FORMAT_SIZE_T "\n", stat->available_space);
    printf("========================\n");
} */