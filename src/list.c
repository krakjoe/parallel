/*
  +----------------------------------------------------------------------+
  | parallel                                                              |
  +----------------------------------------------------------------------+
  | Copyright (c) Joe Watkins 2019                                       |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.01 of the PHP license,      |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.php.net/license/3_01.txt                                  |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Author: krakjoe                                                      |
  +----------------------------------------------------------------------+
 */
#ifndef HAVE_PARALLEL_LIST
#define HAVE_PARALLEL_LIST

#include "parallel.h"

typedef struct _php_parallel_list_el_t php_parallel_list_el_t;

struct _php_parallel_list_el_t {
    void *item;
    php_parallel_list_el_t *prev;
    php_parallel_list_el_t *next;
};

struct _php_parallel_list_t {
    void (*dtor)(void *);
    zend_ulong size;
    zend_ulong count;

    php_parallel_list_el_t *head;
    php_parallel_list_el_t *tail;
};

php_parallel_list_t* php_parallel_list_create(zend_ulong size, llist_dtor_func_t dtor) {
    php_parallel_list_t *list =
        (php_parallel_list_t*)
            php_parallel_heap_alloc(sizeof(php_parallel_list_t));

    list->dtor = dtor;
    list->size = size;
    list->count = 0;

    return list;
}

zend_ulong php_parallel_list_count(php_parallel_list_t *list) {
    return list->count;
}

void* php_parallel_list_head(php_parallel_list_t *list) {
    if (list->head) {
        return list->head->item;
    }

    return NULL;
}

void* php_parallel_list_tail(php_parallel_list_t *list) {
    if (list->tail) {
        return list->tail->item;
    }

    return NULL;
}

void php_parallel_list_apply(php_parallel_list_t *list, llist_apply_func_t func) {
    php_parallel_list_el_t *el = list->head;

    while (el) {
        func(el->item);

        el = el->next;
    }
}

void* php_parallel_list_append(php_parallel_list_t *list, void *mem) {
    php_parallel_list_el_t *el =
        php_parallel_heap_alloc(sizeof(php_parallel_list_el_t) + list->size);

    el->item = (void*) (((char*)el) + sizeof(php_parallel_list_el_t));

    memcpy(el->item, mem, list->size);

    el->prev = list->tail;
    el->next = NULL;

    if (list->tail) {
        list->tail->next = el;
    } else {
        list->head = el;
    }

    list->tail = el;
    list->count++;

    return el->item;
}

static zend_always_inline void php_parallel_list_delete_el(php_parallel_list_t *list, php_parallel_list_el_t *el) {
    if (el->prev) {
        el->prev->next = el->next;
    } else {
        list->head = el->next;
    }

    if (el->next) {
        el->next->prev = el->prev;
    } else {
        list->tail = el->prev;
    }

    if (list->dtor) {
        list->dtor(el->item);
    }

    php_parallel_heap_free(el);
    list->count--;
}

void php_parallel_list_delete(php_parallel_list_t *list, void *mem) {
    php_parallel_list_el_t *el = list->head;

    while (el) {
        if (el->item == mem) {
            php_parallel_list_delete_el(list, el);
            break;
        }
        el = el->next;
    }
}

void php_parallel_list_destroy(php_parallel_list_t *list) {
    php_parallel_list_el_t *el = list->head, *current;

    while (el && (current = el)) {

        if (list->dtor) {
            list->dtor(el->item);
        }

        el = el->next;

        php_parallel_heap_free(current);
    }

    list->head = NULL;
    list->tail = NULL;
}

void php_parallel_list_free(php_parallel_list_t *list) {
    php_parallel_heap_free(list);
}

#endif
