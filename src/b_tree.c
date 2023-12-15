#include "b_tree.h"
#include "buffer_pool.h"

#include <stdio.h>

void b_tree_init(const char *filename, BufferPool *pool) {
    init_buffer_pool(filename, pool);
    /* TODO: add code here */
    if (pool->file.length == 0) {
        Page page = EMPTY_PAGE;
        BCtrlBlock* ctrl = (BCtrlBlock *)&page;
        BNode* bnode = (BNode *)&page;
        ctrl->root_node = PAGE_SIZE;
        ctrl->free_node_head = -1;
        write_page(&page, &pool->file, 0);
        bnode->n = 0;
        bnode->next = -1;
        for (int i = 0; i < 2 * DEGREE + 1; i++)
            bnode->child[i] = -1;
        for (int i = 0; i < 2 * DEGREE; i++) {
            get_rid_block_addr(bnode->row_ptr[i]) = -1;
            get_rid_idx(bnode->row_ptr) = 0;
        }
        bnode->leaf = 1;
        write_page(&page, &pool->file, PAGE_SIZE);
    }
}

void b_tree_close(BufferPool *pool) {
    close_buffer_pool(pool);
}

RID b_tree_search(BufferPool *pool, void *key, size_t size, b_tree_ptr_row_cmp_t cmp) {
    BNode *bnode;
    BCtrlBlock* ctrl;
    RID rid;
    ctrl = (BCtrlBlock *)get_page(pool, 0);
    bnode = (BNode *)get_page(pool, ctrl->root_node);
    rid = tree_search(pool, bnode, key, size, cmp);
    release(pool, ctrl->root_node);
    release(pool, 0);
    return rid;
}

RID b_tree_insert(BufferPool *pool, RID rid, b_tree_row_row_cmp_t cmp, b_tree_insert_nonleaf_handler_t insert_handler) {
    off_t temp;
    BNode *bnode;
    BCtrlBlock *ctrl;
    ChildEntry entry_null;
    ChildEntry* newchildentry;
    RID rid_null;
    get_rid_block_addr(rid_null) = -1;
    get_rid_idx(rid_null) = 0;
    entry_null.addr = -1;
    entry_null.rid = rid_null;
    newchildentry = &entry_null;
    ctrl = (BCtrlBlock *)get_page(pool, 0);
    bnode = (BNode *)get_page(pool, ctrl->root_node);
    temp = ctrl->root_node;
    tree_insert(pool, bnode, rid, newchildentry, cmp, insert_handler);
    if (newchildentry->addr != -1) {
        if (ctrl->free_node_head == -1) {
            Page page = EMPTY_PAGE;
            bnode = &page;
            bnode->n = 1;
            bnode->leaf = 0;
            bnode->next = -1;
            for (int i = 0; i < 2 * DEGREE + 1; i++)
                bnode->child[i] = -1;
            for (int i = 0; i < 2 * DEGREE; i++)
                bnode->row_ptr[i] = rid_null;
            bnode->row_ptr[0] = newchildentry->rid;
            bnode->child[0] = ctrl->root_node;
            bnode->child[1] = newchildentry->addr;
            ctrl->root_node = pool->file.length;
            write_page(&page, &pool->file, pool->file.length);
        }
        else {
            bnode = (BNode *)get_page(pool, ctrl->free_node_head);
            bnode->n = 1;
            bnode->leaf = 0;
            for (int i = 0; i < 2 * DEGREE + 1; i++)
                bnode->child[i] = -1;
            for (int i = 0; i < 2 * DEGREE; i++)
                bnode->row_ptr[i] = rid_null;
            bnode->row_ptr[0] = newchildentry->rid;
            bnode->child[0] = ctrl->root_node;
            bnode->child[1] = newchildentry->addr;
            ctrl->root_node = ctrl->free_node_head;
            ctrl->free_node_head = bnode->next;
            bnode->next = -1;
            release(pool, ctrl->root_node);
        }
    }
    release(pool, temp);
    release(pool, 0);
    return rid;
}

void b_tree_delete(BufferPool *pool, RID rid, b_tree_row_row_cmp_t cmp, b_tree_insert_nonleaf_handler_t insert_handler, b_tree_delete_nonleaf_handler_t delete_handler) {
    int index;
    off_t temp, temp_1;
    BNode *parent, *child;
    BCtrlBlock *ctrl;
    RID rid_null, childentry;
    RID *oldchildentry = &childentry;
    get_rid_block_addr(rid_null) = -1;
    get_rid_idx(rid_null) = 0;
    childentry = rid_null;
    ctrl = (BCtrlBlock *)get_page(pool, 0);
    parent = (BNode *)get_page(pool, ctrl->root_node);
    temp = ctrl->root_node;
    index = find_index(parent, rid, 1, cmp);
    if (parent->leaf) {
        for (int i = index - 1; i < parent->n - 1; i++)
            parent->row_ptr[i] = parent->row_ptr[i + 1];
        parent->row_ptr[parent->n - 1] = rid_null;
        parent->n--;
    }
    else {
        child = (BNode *)get_page(pool, parent->child[index]);
        temp_1 = parent->child[index];
        tree_delete(pool, parent, child, rid, oldchildentry, cmp, insert_handler, delete_handler);
        if (get_rid_block_addr(*oldchildentry) != -1) {
            if (parent->n > 1) {
                index = find_index(parent, *oldchildentry, 0, cmp);
                delete_handler(*oldchildentry);
                for (int i = index - 1; i < parent->n - 1; i++)
                    parent->row_ptr[i] = parent->row_ptr[i + 1];
                parent->row_ptr[parent->n - 1] = rid_null;
                for (int i = index; i < parent->n; i++)
                    parent->child[i] = parent->child[i + 1];
                parent->child[parent->n] = -1;
                parent->n--;
            }
            else {
                delete_handler(*oldchildentry);
                parent->n--;
                parent->next = ctrl->free_node_head;
                ctrl->free_node_head = ctrl->root_node;
                ctrl->root_node = parent->child[0];
            }
        }
        release(pool, temp_1);
    }
    release(pool, temp);
    release(pool, 0);
}

RID tree_search(BufferPool *pool, BNode *bnode, void *key, size_t size, b_tree_ptr_row_cmp_t cmp) {
    BNode* node;
    RID rid, rid_null;
    get_rid_block_addr(rid_null) = -1;
    get_rid_idx(rid_null) = 0;
    if (bnode->leaf) {
        for (int i = 0; i < bnode->n; i++)
            if (cmp(key, size, bnode->row_ptr[i], 1) == 0)
                return bnode->row_ptr[i];
        return rid_null;
    }
    if (cmp(key, size, bnode->row_ptr[0], 0) < 0) {
        node = (BNode *)get_page(pool, bnode->child[0]);
        rid = tree_search(pool, node, key, size, cmp);
        release(pool, bnode->child[0]);
        return rid;
    }
    for (int i = 0; i < bnode->n - 1; i++) {
        if (cmp(key, size, bnode->row_ptr[i], 0) >= 0 && cmp(key, size, bnode->row_ptr[i + 1], 0) < 0) {
            node = (BNode *)get_page(pool, bnode->child[i + 1]);
            rid = tree_search(pool, node, key, size, cmp);
            release(pool, bnode->child[i + 1]);
            return rid;
        }
    }
    if (cmp(key, size, bnode->row_ptr[bnode->n - 1], 0) >= 0) {
        node = (BNode *)get_page(pool, bnode->child[bnode->n]);
        rid = tree_search(pool, node, key, size, cmp);
        release(pool, bnode->child[bnode->n]);
        return rid;
    }
}

void tree_insert(BufferPool *pool, BNode *bnode, RID rid, ChildEntry *newchildentry, b_tree_row_row_cmp_t cmp, b_tree_insert_nonleaf_handler_t insert_handler) {
    int index;
    RID rid_null, rid_temp;
    BCtrlBlock* ctrl;
    BNode *node;
    get_rid_block_addr(rid_null) = -1;
    get_rid_idx(rid_null) = 0;
    ctrl = (BCtrlBlock *)get_page(pool, 0);
    index = find_index(bnode, rid, 1, cmp);
    if (bnode->leaf) {
        if (bnode->n < 2 * DEGREE) {
            for (int i = bnode->n - 1; i >= index; i--)
                bnode->row_ptr[i + 1] = bnode->row_ptr[i];
            bnode->row_ptr[index] = rid;
            newchildentry->addr = -1;;
            bnode->n++;
        }
        else {
            if (ctrl->free_node_head == -1) {
                Page page = EMPTY_PAGE;
                node = (BNode *)&page;
                node->n = DEGREE + 1;
                node->leaf = 1;
                for (int i = 0; i < 2 * DEGREE + 1; i++)
                    node->child[i] = -1;
                for (int i = 0; i < 2 * DEGREE; i++)
                    node->row_ptr[i] = rid_null;
                if (index >= DEGREE) {
                    for (int i = 0; i < index - DEGREE; i++)
                        node->row_ptr[i] = bnode->row_ptr[i + DEGREE];
                    node->row_ptr[index - DEGREE] = rid;
                    for (int i = index - DEGREE + 1; i < DEGREE + 1; i++)
                        node->row_ptr[i] = bnode->row_ptr[i + DEGREE - 1];
                    for (int i = DEGREE; i < 2 * DEGREE; i++)
                        bnode->row_ptr[i] = rid_null;
                }
                else {
                    for (int i = 0; i < DEGREE + 1; i++) {
                        node->row_ptr[i] = bnode->row_ptr[i + DEGREE - 1];
                        bnode->row_ptr[i + DEGREE - 1] = rid_null;
                    }
                    for (int i = DEGREE - 2; i >= index; i--)
                        bnode->row_ptr[i + 1] = bnode->row_ptr[i];
                    bnode->row_ptr[index] = rid;
                }
                bnode->n = DEGREE;
                node->next = bnode->next;
                bnode->next = pool->file.length;
                newchildentry->addr = pool->file.length;
                newchildentry->rid = insert_handler(node->row_ptr[0], 1);
                write_page(&page, &pool->file, pool->file.length);
            }
            else {
                node = (BNode *)get_page(pool, ctrl->free_node_head);
                newchildentry->addr = ctrl->free_node_head;
                ctrl->free_node_head = node->next;
                node->n = DEGREE + 1;
                node->leaf = 1;
                for (int i = 0; i < 2 * DEGREE + 1; i++)
                    node->child[i] = -1;
                for (int i = 0; i < 2 * DEGREE; i++)
                    node->row_ptr[i] = rid_null;
                if (index >= DEGREE) {
                    for (int i = 0; i < index - DEGREE; i++)
                        node->row_ptr[i] = bnode->row_ptr[i + DEGREE];
                    node->row_ptr[index - DEGREE] = rid;
                    for (int i = index - DEGREE + 1; i < DEGREE + 1; i++)
                        node->row_ptr[i] = bnode->row_ptr[i + DEGREE - 1];
                    for (int i = DEGREE; i < 2 * DEGREE; i++)
                        bnode->row_ptr[i] = rid_null;
                }
                else {
                    for (int i = 0; i < DEGREE + 1; i++) {
                        node->row_ptr[i] = bnode->row_ptr[i + DEGREE - 1];
                        bnode->row_ptr[i + DEGREE - 1] = rid_null;
                    }
                    for (int i = DEGREE - 2; i >= index; i--)
                        bnode->row_ptr[i + 1] = bnode->row_ptr[i];
                    bnode->row_ptr[index] = rid;
                }
                bnode->n = DEGREE;
                node->next = bnode->next;
                bnode->next = newchildentry->addr;
                newchildentry->rid = insert_handler(node->row_ptr[0], 1);
                release(pool, newchildentry->addr);
            }
        }
    }
    else {
        node = (BNode *)get_page(pool, bnode->child[index]);
        tree_insert(pool, node, rid, newchildentry, cmp, insert_handler);
        release(pool, bnode->child[index]);
        if (newchildentry->addr == -1) {
            release(pool, 0);
            return;
        }
        if (bnode->n < 2 * DEGREE) {
            for (int i = bnode->n - 1; i >= index; i--)
                bnode->row_ptr[i + 1] = bnode->row_ptr[i];
            for (int i = bnode->n; i > index; i--)
                bnode->child[i + 1] = bnode->child[i];
            bnode->child[index + 1] = newchildentry->addr;
            bnode->row_ptr[index] = newchildentry->rid;
            bnode->n++;
            newchildentry->addr = -1;
        }
        else {
            if (ctrl->free_node_head == -1) {
                Page page = EMPTY_PAGE;
                node = (BNode *)&page;
                node->n = DEGREE;
                node->leaf = 0;
                node->next = -1;
                for (int i = 0; i < 2 * DEGREE + 1; i++)
                    node->child[i] = -1;
                for (int i = 0; i < 2 * DEGREE; i++)
                    node->row_ptr[i] = rid_null;
                if (index > DEGREE) {
                    rid_temp = bnode->row_ptr[DEGREE];
                    for (int i = 0; i < index - DEGREE - 1; i++)
                        node->row_ptr[i] = bnode->row_ptr[i + DEGREE + 1];
                    node->row_ptr[index - DEGREE - 1] = newchildentry->rid;
                    for (int i = index - DEGREE; i < DEGREE; i++)
                        node->row_ptr[i] = bnode->row_ptr[i + DEGREE];
                    for (int i = DEGREE; i < 2 * DEGREE; i++)
                        bnode->row_ptr[i] = rid_null;
                    for (int i = 0; i < index - DEGREE; i++)
                        node->child[i] = bnode->child[i + DEGREE + 1];
                    node->child[index - DEGREE] = newchildentry->addr;
                    for (int i = index - DEGREE + 1; i < DEGREE + 1; i++)
                        node->child[i] = bnode->child[i + DEGREE];
                    for (int i = DEGREE + 1; i < 2 * DEGREE; i++)
                        node->child[i] = -1;
                    newchildentry->rid = rid_temp;
                }
                if (index == DEGREE) {
                    for (int i = 0; i < DEGREE; i++) {
                        node->row_ptr[i] = bnode->row_ptr[i + DEGREE];
                        bnode->row_ptr[i + DEGREE] = rid_null;
                    }
                    node->child[0] = newchildentry->addr;
                    for (int i = 1; i < DEGREE + 1; i++)
                        node->child[i] = bnode->child[i + DEGREE];
                }
                if (index < DEGREE) {
                    rid_temp = bnode->row_ptr[DEGREE - 1];
                    for (int i = 0; i < DEGREE; i++) {
                        node->row_ptr[i] = bnode->row_ptr[i + DEGREE];
                        bnode->row_ptr[i + DEGREE] = rid_null;
                    }
                    for (int i = 0; i < DEGREE + 1; i++)
                        node->child[i] = bnode->child[i + DEGREE];
                    for (int i = DEGREE - 1; i > index; i--)
                        bnode->row_ptr[i] = bnode->row_ptr[i - 1];
                    bnode->row_ptr[index] = newchildentry->rid;
                    for (int i = DEGREE; i > index + 1; i--)
                        bnode->child[i] = bnode->child[i - 1];
                    bnode->child[index + 1] = newchildentry->addr;
                    newchildentry->rid = rid_temp;
                }
                bnode->n = DEGREE;
                newchildentry->addr = pool->file.length;
                write_page(&page, &pool->file, pool->file.length);
            }
            else {
                node = (BNode *)get_page(pool, ctrl->free_node_head);
                node->n = DEGREE;
                node->leaf = 0;
                for (int i = 0; i < 2 * DEGREE + 1; i++)
                    node->child[i] = -1;
                for (int i = 0; i < 2 * DEGREE; i++)
                    node->row_ptr[i] = rid_null;
                if (index > DEGREE) {
                    rid_temp = bnode->row_ptr[DEGREE];
                    for (int i = 0; i < index - DEGREE - 1; i++)
                        node->row_ptr[i] = bnode->row_ptr[i + DEGREE + 1];
                    node->row_ptr[index - DEGREE - 1] = newchildentry->rid;
                    for (int i = index - DEGREE; i < DEGREE; i++)
                        node->row_ptr[i] = bnode->row_ptr[i + DEGREE];
                    for (int i = DEGREE; i < 2 * DEGREE; i++)
                        bnode->row_ptr[i] = rid_null;
                    for (int i = 0; i < index - DEGREE; i++)
                        node->child[i] = bnode->child[i + DEGREE + 1];
                    node->child[index - DEGREE] = newchildentry->addr;
                    for (int i = index - DEGREE + 1; i < DEGREE + 1; i++)
                        node->child[i] = bnode->child[i + DEGREE];
                    for (int i = DEGREE + 1; i < 2 * DEGREE; i++)
                        node->child[i] = -1;
                    newchildentry->rid = rid_temp;
                }
                if (index == DEGREE) {
                    for (int i = 0; i < DEGREE; i++) {
                        node->row_ptr[i] = bnode->row_ptr[i + DEGREE];
                        bnode->row_ptr[i + DEGREE] = rid_null;
                    }
                    node->child[0] = newchildentry->addr;
                    for (int i = 1; i < DEGREE + 1; i++)
                        node->child[i] = bnode->child[i + DEGREE];
                }
                if (index < DEGREE) {
                    rid_temp = bnode->row_ptr[DEGREE - 1];
                    for (int i = 0; i < DEGREE; i++) {
                        node->row_ptr[i] = bnode->row_ptr[i + DEGREE];
                        bnode->row_ptr[i + DEGREE] = rid_null;
                    }
                    for (int i = 0; i < DEGREE + 1; i++)
                        node->child[i] = bnode->child[i + DEGREE];
                    for (int i = DEGREE - 1; i > index; i--)
                        bnode->row_ptr[i] = bnode->row_ptr[i - 1];
                    bnode->row_ptr[index] = newchildentry->rid;
                    for (int i = DEGREE; i > index + 1; i--)
                        bnode->child[i] = bnode->child[i - 1];
                    bnode->child[index + 1] = newchildentry->addr;
                    newchildentry->rid = rid_temp;
                }
                bnode->n = DEGREE;
                newchildentry->addr = ctrl->free_node_head;
                ctrl->free_node_head = node->next;
                node->next = -1;
                release(pool, newchildentry->addr);
            }
        }
    }
    release(pool, 0);
}

void tree_delete(BufferPool *pool, BNode *parent, BNode *child, RID rid, RID *oldchildentry, b_tree_row_row_cmp_t cmp, b_tree_insert_nonleaf_handler_t insert_handler, b_tree_delete_nonleaf_handler_t delete_handler) {
    int index, index_1;
    BCtrlBlock* ctrl;
    BNode* sibling, *grandson;
    RID rid_null;
    get_rid_block_addr(rid_null) = -1;
    get_rid_idx(rid_null) = 0;
    ctrl = (BCtrlBlock *)get_page(pool, 0);
    index = find_index(child, rid, 1, cmp);
    if (child->leaf) {
        if (child->n > DEGREE) {
            for (int i = index - 1; i < child->n - 1; i++)
                child->row_ptr[i] = child->row_ptr[i + 1];
            child->row_ptr[child->n - 1] = rid_null;
            child->n--;
            *oldchildentry = rid_null;
        }
        else {
            index_1 = find_index(parent, child->row_ptr[0], child->leaf, cmp);
            if (index_1 != parent->n) {
                sibling = (BNode *)get_page(pool, parent->child[index_1 + 1]);
                for (int i = index - 1; i < DEGREE - 1; i++)
                    child->row_ptr[i] = child->row_ptr[i + 1];
                if (sibling->n > DEGREE) {
                    child->row_ptr[DEGREE - 1] = sibling->row_ptr[0];
                    for (int i = 0; i < sibling->n - 1; i++)
                        sibling->row_ptr[i] = sibling->row_ptr[i + 1];
                    sibling->row_ptr[sibling->n - 1] = rid_null;
                    sibling->n--;
                    delete_handler(parent->row_ptr[index_1]);
                    parent->row_ptr[index_1] = insert_handler(sibling->row_ptr[0], 1);
                    *oldchildentry = rid_null;
                }
                else {
                    for (int i = DEGREE - 1; i < 2 * DEGREE - 1; i++)
                        child->row_ptr[i] = sibling->row_ptr[i - DEGREE + 1];
                    *oldchildentry = parent->row_ptr[index_1];
                    child->next = sibling->next;
                    sibling->next = ctrl->free_node_head;
                    child->n = 2 * DEGREE - 1;
                    ctrl->free_node_head = parent->child[index_1 + 1];
                }
                release(pool, parent->child[index_1 + 1]);
            }
            else {
                sibling = (BNode *)get_page(pool, parent->child[index_1 - 1]);
                for (int i = index - 1; i > 0; i--)
                    child->row_ptr[i] = child->row_ptr[i - 1];
                if (sibling->n > DEGREE) {
                    child->row_ptr[0] = sibling->row_ptr[sibling->n - 1];
                    sibling->row_ptr[sibling->n - 1] = rid_null;
                    sibling->n--;
                    delete_handler(parent->row_ptr[index_1 - 1]);
                    parent->row_ptr[index_1 - 1] = insert_handler(child->row_ptr[0], 1);
                    *oldchildentry = rid_null;
                }
                else {
                    for (int i = DEGREE; i < 2 * DEGREE - 1; i++)
                        sibling->row_ptr[i] = child->row_ptr[i - DEGREE + 1];
                    *oldchildentry = parent->row_ptr[index_1 - 1];
                    sibling->next = child->next;
                    sibling->n = 2 * DEGREE - 1;
                    child->next = ctrl->free_node_head;
                    ctrl->free_node_head = parent->child[index_1];
                }
                release(pool, parent->child[index_1 - 1]);
            }
        }
    }
    else {
        grandson = (BNode *)get_page(pool, child->child[index]);
        tree_delete(pool, child, grandson, rid, oldchildentry, cmp, insert_handler, delete_handler);
        release(pool, child->child[index]);
        if (get_rid_block_addr(*oldchildentry) == -1) {
            release(pool, 0);
            return;
        }
        index_1 = find_index(child, *oldchildentry, 0, cmp);
        delete_handler(*oldchildentry);
        for (int i = index_1 - 1; i < child->n - 1; i++)
            child->row_ptr[i] = child->row_ptr[i + 1];
        child->row_ptr[child->n - 1] = rid_null;
        for (int i = index_1; i < child->n; i++)
            child->child[i] = child->child[i + 1];
        child->child[child->n] = -1;
        child->n--;
        if (child->n >= DEGREE)
            *oldchildentry = rid_null;
        else {
            index_1 = find_index(parent, child->row_ptr[0], child->leaf, cmp);
            if (index_1 != parent->n) {
                sibling = (BNode *)get_page(pool, parent->child[index_1 + 1]);
                if (sibling->n > DEGREE) {
                    child->row_ptr[child->n] = parent->row_ptr[index_1];
                    child->child[child->n + 1] = sibling->child[0];
                    child->n++;
                    parent->row_ptr[index_1] = sibling->row_ptr[0];
                    for (int i = 0; i < sibling->n - 1; i++)
                        sibling->row_ptr[i] = sibling->row_ptr[i + 1];
                    sibling->row_ptr[sibling->n - 1] = rid_null;
                    for (int i = 0; i < sibling->n; i++)
                        sibling->child[i] = sibling->child[i + 1];
                    sibling->child[sibling->n] = -1;
                    sibling->n--;
                    *oldchildentry = rid_null;
                }
                else {
                    *oldchildentry = parent->row_ptr[index_1];
                    child->row_ptr[child->n] = insert_handler(parent->row_ptr[index_1], 0);
                    for (int i = DEGREE; i < 2 * DEGREE; i++)
                        child->row_ptr[i] = sibling->row_ptr[i - DEGREE];
                    for (int i = DEGREE; i < 2 * DEGREE + 1; i++)
                        child->child[i] = sibling->child[i - DEGREE];
                    child->next = sibling->next;
                    child->n = 2 * DEGREE;
                    sibling->next = ctrl->free_node_head;
                    ctrl->free_node_head = parent->child[index_1 + 1];
                }
                release(pool, parent->child[index_1 + 1]);
            }
            else {
                sibling = (BNode *)get_page(pool, parent->child[index_1 - 1]);
                if (sibling->n > DEGREE) {
                    for (int i = DEGREE - 1; i > 0; i--)
                        child->row_ptr[i] = child->row_ptr[i - 1];
                    for (int i = DEGREE; i > 0; i--)
                        child->child[i] = child->child[i - 1];
                    child->row_ptr[0] = parent->row_ptr[index_1 - 1];
                    child->child[0] = sibling->child[sibling->n];
                    child->n++;
                    parent->row_ptr[index_1 - 1] = sibling->row_ptr[sibling->n - 1];
                    sibling->row_ptr[sibling->n - 1] = rid_null;
                    sibling->child[sibling->n] = -1;
                    sibling->n--;
                    *oldchildentry = rid_null;
                }
                else {
                    *oldchildentry = parent->row_ptr[index_1 - 1];
                    sibling->row_ptr[DEGREE] = insert_handler(parent->row_ptr[index_1 - 1], 0);
                    for (int i = DEGREE + 1; i < 2 * DEGREE; i++)
                        sibling->row_ptr[i] = child->row_ptr[i - DEGREE - 1];
                    for (int i = DEGREE + 1; i < 2 * DEGREE + 1; i++)
                        sibling->child[i] = child->child[i - DEGREE - 1];
                    sibling->next = child->next;
                    sibling->n = 2 * DEGREE;
                    child->next = ctrl->free_node_head;
                    ctrl->free_node_head = parent->child[index_1];
                }
                release(pool, parent->child[index_1 - 1]);
            }
        }
    }
    release(pool, 0);
}

int find_index(BNode *bnode, RID rid, int rid_leaf, b_tree_row_row_cmp_t cmp) {
    if (bnode->n == 0)
        return 0;
    if (cmp(rid, bnode->row_ptr[0], rid_leaf, bnode->leaf) < 0)
        return 0;
    for (int i = 0; i < bnode->n - 1; i++) {
        if (cmp(rid, bnode->row_ptr[i], rid_leaf, bnode->leaf) >= 0 && cmp(rid, bnode->row_ptr[i + 1], rid_leaf, bnode->leaf) < 0)
            return i + 1;
    }
    if (cmp(rid, bnode->row_ptr[bnode->n - 1], rid_leaf, bnode->leaf) >= 0)
        return bnode->n;
}