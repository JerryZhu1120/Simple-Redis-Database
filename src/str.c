#include "str.h"

#include "table.h"

void read_string(Table* table, RID rid, StringRecord* record) {
    table_read(table, rid, record->chunk.data);
    record->idx = 0;
}

int has_next_char(StringRecord* record) {
    short size;
    RID rid;
    size = get_str_chunk_size(&record->chunk);
    rid = get_str_chunk_rid(&record->chunk);
    if ((record->idx != size) || get_rid_block_addr(rid) != -1)
        return 1;
    else
        return 0;
}

char next_char(Table* table, StringRecord* record) {
    RID rid;
    if (record->idx != get_str_chunk_size(&record->chunk))
        return get_str_chunk_data_ptr(&record->chunk)[record->idx++];
    rid = get_str_chunk_rid(&record->chunk);
    read_string(table, rid, record);
    return get_str_chunk_data_ptr(&record->chunk)[record->idx++];
}

int compare_string_record(Table* table, const StringRecord* a, const StringRecord* b) {
    StringRecord aa, bb;
    char x, y;
    aa = *a;
    bb = *b;
    do {
        if (has_next_char(&aa) && has_next_char(&bb)) {
            x = next_char(table, &aa);
            y = next_char(table, &bb);
        }
        else if (has_next_char(&aa))
            return 1;
        else if (has_next_char(&bb))
            return -1;
        else
            return 0;
    } while (x == y);
    if (x < y)
        return -1;
    else
        return 1;
}

RID write_string(Table* table, const char* data, off_t size) {
   char* ptr;
   short last_chunk_length;
   StringChunk chunk;
   RID rid;
   if (size == 0) {
       get_rid_block_addr(rid) = -1;
       get_rid_idx(rid) = 0;
       get_str_chunk_rid(&chunk) = rid;
       get_str_chunk_size(&chunk) = size;
       rid = table_insert(table, (ItemPtr)&chunk, calc_str_chunk_size(size));
       return rid;
   }
   last_chunk_length = size % STR_CHUNK_MAX_LEN;
   if (last_chunk_length != 0) {
       ptr = data + size - last_chunk_length;
       get_rid_block_addr(rid) = -1;
       get_rid_idx(rid) = 0;
       get_str_chunk_rid(&chunk) = rid;
       get_str_chunk_size(&chunk) = last_chunk_length;
       for (int i = 0; i < last_chunk_length; i++)
           get_str_chunk_data_ptr(&chunk)[i] = ptr[i];
       rid = table_insert(table, (ItemPtr)&chunk, calc_str_chunk_size(last_chunk_length));
       for (int i = 0; i < size / STR_CHUNK_MAX_LEN; i++) {
           ptr -= STR_CHUNK_MAX_LEN;
           get_str_chunk_rid(&chunk) = rid;
           get_str_chunk_size(&chunk) = STR_CHUNK_MAX_LEN;
           for (int j = 0; j < STR_CHUNK_MAX_LEN; j++)
               get_str_chunk_data_ptr(&chunk)[j] = ptr[j];
           rid = table_insert(table, (ItemPtr)&chunk, STR_CHUNK_MAX_SIZE);
       }
   }
   else {
       ptr = data + size - STR_CHUNK_MAX_LEN;
       get_rid_block_addr(rid) = -1;
       get_rid_idx(rid) = 0;
       get_str_chunk_rid(&chunk) = rid;
       get_str_chunk_size(&chunk) = STR_CHUNK_MAX_LEN;
       for (int i = 0; i < STR_CHUNK_MAX_LEN; i++)
           get_str_chunk_data_ptr(&chunk)[i] = ptr[i];
       rid = table_insert(table, (ItemPtr)&chunk, STR_CHUNK_MAX_SIZE);
       for (int i = 0; i < size / STR_CHUNK_MAX_LEN - 1; i++) {
           ptr -= STR_CHUNK_MAX_LEN;
           get_str_chunk_rid(&chunk) = rid;
           get_str_chunk_size(&chunk) = STR_CHUNK_MAX_LEN;
           for (int j = 0; j < STR_CHUNK_MAX_LEN; j++)
               get_str_chunk_data_ptr(&chunk)[j] = ptr[j];
           rid = table_insert(table, (ItemPtr)&chunk, STR_CHUNK_MAX_SIZE);
       }
   }
   return rid;
}

void delete_string(Table* table, RID rid) {
    StringChunk chunk;
    while (get_rid_block_addr(rid) != -1) {
        table_read(table, rid, chunk.data);
        table_delete(table, rid);
        rid = get_str_chunk_rid(&chunk);
    }
}

/* void print_string(Table *table, const StringRecord *record) {
    StringRecord rec = *record;
    printf("\"");
    while (has_next_char(&rec)) {
        printf("%c", next_char(table, &rec));
    }
    printf("\"");
} */

size_t load_string(Table* table, const StringRecord* record, char* dest, size_t max_size) {
    size_t count = 0;
    StringRecord strrcd = *record;
    while (count < max_size) {
        if (has_next_char(&strrcd) == 0)
            return count;
        dest[count++] = next_char(table, &strrcd);
    }
    return count;
}

/* void chunk_printer(ItemPtr item, short item_size) {
    if (item == NULL) {
        printf("NULL");
        return;
    }
    StringChunk *chunk = (StringChunk*)item;
    short size = get_str_chunk_size(chunk), i;
    printf("StringChunk(");
    print_rid(get_str_chunk_rid(chunk));
    printf(", %d, \"", size);
    for (i = 0; i < size; i++) {
        printf("%c", get_str_chunk_data_ptr(chunk)[i]);
    }
    printf("\")");
} */