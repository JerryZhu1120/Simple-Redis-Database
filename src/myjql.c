#include "myjql.h"

#include "buffer_pool.h"
#include "b_tree.h"
#include "table.h"
#include "str.h"

#define MAX_STR_LEN 1024

typedef struct {
    RID key;
    RID value;
} Record;

void read_record(Table *table, RID rid, Record *record) {
    table_read(table, rid, (ItemPtr)record);
}

RID write_record(Table *table, const Record *record) {
    return table_insert(table, (ItemPtr)record, sizeof(Record));
}

void delete_record(Table *table, RID rid) {
    table_delete(table, rid);
}

BufferPool bp_idx;
Table tbl_rec;
Table tbl_str;

int rid_row_row_cmp(RID a, RID b, int aleaf, int bleaf) {
    Record A, B;
    StringRecord AA, BB;
    if (aleaf) {
        read_record(&tbl_rec, a, &A);
        read_string(&tbl_str, A.key, &AA);
    }
    else
        read_string(&tbl_str, a, &AA);
    if (bleaf) {
        read_record(&tbl_rec, b, &B);
        read_string(&tbl_str, B.key, &BB);
    }
    else
        read_string(&tbl_str, b, &BB);
    return compare_string_record(&tbl_str, &AA, &BB);
}

int rid_ptr_row_cmp(void* p, size_t size, RID b, int leaf) {
    int ans;
    RID rid;
    Record B;
    StringRecord AA, BB;
    rid = write_string(&tbl_str, p, size);
    read_string(&tbl_str, rid, &AA);
    if (leaf) {
        read_record(&tbl_rec, b, &B);
        read_string(&tbl_str, B.key, &BB);
    }
    else
        read_string(&tbl_str, b, &BB);
    ans = compare_string_record(&tbl_str, &AA, &BB);
    delete_string(&tbl_str, rid);
    return ans;
}

RID insert_handler(RID rid, int leaf) {
    size_t size;
    char p[MAX_STR_LEN];
    Record A;
    StringRecord AA;
    if (leaf) {
        read_record(&tbl_rec, rid, &A);
        read_string(&tbl_str, A.key, &AA);
    }
    else
        read_string(&tbl_str, rid, &AA);
    size = load_string(&tbl_str, &AA, p, MAX_STR_LEN);
    return write_string(&tbl_str, p, size);
}

void delete_handler(RID rid) {
    delete_string(&tbl_str, rid);
}

void myjql_init() {
    b_tree_init("rec.idx", &bp_idx);
    table_init(&tbl_rec, "rec.data", "rec.fsm");
    table_init(&tbl_str, "str.data", "str.fsm");
}

void myjql_close() {
    /* validate_buffer_pool(&bp_idx);
    validate_buffer_pool(&tbl_rec.data_pool);
    validate_buffer_pool(&tbl_rec.fsm_pool);
    validate_buffer_pool(&tbl_str.data_pool);
    validate_buffer_pool(&tbl_str.fsm_pool); */
    b_tree_close(&bp_idx);
    table_close(&tbl_rec);
    table_close(&tbl_str);
}

size_t myjql_get(const char *key, size_t key_len, char *value, size_t max_size) {
    RID rid;
    Record record;
    StringRecord strrecord;
    rid = b_tree_search(&bp_idx, key, key_len, rid_ptr_row_cmp);
    if (get_rid_block_addr(rid) == -1 && get_rid_idx(rid) == 0)
        return -1;
    read_record(&tbl_rec, rid, &record);
    rid = record.value;
    read_string(&tbl_str, rid, &strrecord);
    return load_string(&tbl_str, &strrecord, value, max_size);
}

void myjql_set(const char *key, size_t key_len, const char *value, size_t value_len) {
    RID rid;
    Record record;
    rid = b_tree_search(&bp_idx, key, key_len, rid_ptr_row_cmp);
    if (get_rid_block_addr(rid) == -1 && get_rid_idx(rid) == 0){
        record.key = write_string(&tbl_str, key, key_len);
        record.value = write_string(&tbl_str, value, value_len);
        rid = write_record(&tbl_rec, &record);
        b_tree_insert(&bp_idx, rid, rid_row_row_cmp, insert_handler);
    }
    else{
        read_record(&tbl_rec, rid, &record);
        b_tree_delete(&bp_idx, rid, rid_row_row_cmp, insert_handler, delete_handler);
        delete_record(&tbl_rec, rid);
        delete_string(&tbl_str, record.value);
        record.value = write_string(&tbl_str, value, value_len);
        rid = write_record(&tbl_rec, &record);
        b_tree_insert(&bp_idx, rid, rid_row_row_cmp, insert_handler);
    }
}

void myjql_del(const char *key, size_t key_len) {
    RID rid;
    Record record;
    rid = b_tree_search(&bp_idx, key, key_len, rid_ptr_row_cmp);
    if (get_rid_block_addr(rid) == -1 && get_rid_idx(rid) == 0)
        return;
    b_tree_delete(&bp_idx, rid, rid_row_row_cmp, insert_handler, delete_handler);
    read_record(&tbl_rec, rid, &record);
    delete_record(&tbl_rec, rid);
    delete_string(&tbl_str, record.key);
    delete_string(&tbl_str, record.value);
}

/* void myjql_analyze() {
    printf("Record Table:\n");
    analyze_table(&tbl_rec);
    printf("String Table:\n");
    analyze_table(&tbl_str);
} */