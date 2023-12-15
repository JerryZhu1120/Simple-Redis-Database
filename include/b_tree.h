#ifndef _B_TREE_H
#define _B_TREE_H

#include "file_io.h"
#include "table.h"

/* it MUST be satisfied that: DEGREE >= 2 */
#define DEGREE (((PAGE_SIZE) - sizeof(size_t) - 2 * sizeof(off_t) - sizeof(int)) / 2 / (sizeof(off_t) + sizeof(RID)))

typedef struct {
  /* B-Tree Node */
  /* you can modify anything in this struct */
  size_t n;
  off_t next;
  off_t child[2 * DEGREE + 1];
  RID row_ptr[2 * DEGREE];
  int leaf;
} BNode;

typedef struct {
  /* B-Tree Control Block */
  /* you can modify anything in this struct */
  off_t root_node;
  off_t free_node_head;
} BCtrlBlock;

typedef struct {
	off_t addr;
	RID rid;
} ChildEntry;

/* BEGIN: --------------------------------- DO NOT MODIFY! --------------------------------- */

typedef int (*b_tree_row_row_cmp_t)(RID, RID, int, int);
typedef int (*b_tree_ptr_row_cmp_t)(void *, size_t, RID, int);

/*
 * when inserting rid into a non-leaf node in b_tree,
 * the key (referenced by rid) might be copied and returned.
 * 
 * most simple logic for the handler:
 * 1) copy the key according to rid;
 * 2) return the rid of the newly-copied key.
 * 
 * example usage:
 * b_tree_insert_nonleaf_handler_t insert_handler;
 * rid to insert: RID rid;
 * current node: BNode *node; (node is NOT a leaf node, i.e. !node->leaf)
 * the position to insert: (signed/unsigned) integer k;
 * code:
 * node->row_ptr[k] = insert_handler(rid);
 * 
 * note: the handler may also be needed for redistribution in deletion
 */
typedef RID (*b_tree_insert_nonleaf_handler_t)(RID rid, int leaf);

/*
 * when deleting rid from a non-leaf node in b_tree,
 * the key (referenced by rid) might be deleted.
 * 
 * most simple logic for the hanlder:
 * delete the key according to rid.
 * 
 * example usage:
 * b_tree_delete_nonleaf_handler_t delete_handler;
 * current node: BNode *node; (node is NOT a leaf node, i.e. !node->leaf)
 * the position to delete: (signed/unsigned) integer k;
 * code:
 * delete_handler(node->row_ptr[k]);
 */
typedef void (*b_tree_delete_nonleaf_handler_t)(RID rid);

void b_tree_init(const char *filename, BufferPool *pool);

void b_tree_close(BufferPool *pool);

RID b_tree_search(BufferPool *pool, void *key, size_t size, b_tree_ptr_row_cmp_t cmp);

RID b_tree_insert(BufferPool *pool, RID rid, b_tree_row_row_cmp_t cmp, b_tree_insert_nonleaf_handler_t insert_handler);

void b_tree_delete(BufferPool *pool, RID rid, b_tree_row_row_cmp_t cmp, b_tree_insert_nonleaf_handler_t insert_handler, b_tree_delete_nonleaf_handler_t delete_handler);

/* END:   --------------------------------- DO NOT MODIFY! --------------------------------- */

RID tree_search(BufferPool *pool, BNode *bnode, void *key, size_t size, b_tree_ptr_row_cmp_t cmp);

void tree_insert(BufferPool *pool, BNode *bnode, RID rid, ChildEntry *newchildentry, b_tree_row_row_cmp_t cmp, b_tree_insert_nonleaf_handler_t insert_handler);

void tree_delete(BufferPool* pool, BNode* parent, BNode* child, RID rid, RID *oldchildentry, b_tree_row_row_cmp_t cmp, b_tree_insert_nonleaf_handler_t insert_handler, b_tree_delete_nonleaf_handler_t delete_handler);

int find_index(BNode* bnode, RID rid, int rid_leaf, b_tree_row_row_cmp_t cmp);

#endif  /* _B_TREE_H */