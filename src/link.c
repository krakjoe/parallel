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
#ifndef HAVE_PARALLEL_LINK
#define HAVE_PARALLEL_LINK

#include "parallel.h"
#include "link.h"

#define PHP_PARALLEL_LINK_CLOSURE_BUFFER GC_IMMUTABLE

typedef enum {
    PHP_PARALLEL_LINK_UNBUFFERED,
    PHP_PARALLEL_LINK_BUFFERED
} php_parallel_link_type_t;

typedef struct {
    pthread_mutex_t m;
    pthread_mutex_t w;
    pthread_mutex_t r;
} php_parallel_link_mutex_t;

typedef struct {
    pthread_cond_t r;
    pthread_cond_t w;
} php_parallel_link_cond_t;

typedef struct {
    uint32_t c;
    uint32_t r;
    uint32_t w;
} php_parallel_link_state_t;

typedef struct _php_parallel_link_queue_t {
    zend_llist l;
    zend_long  c;
} php_parallel_link_queue_t;

struct _php_parallel_link_t {
    php_parallel_link_type_t type;
    zend_string *name;

    php_parallel_link_mutex_t m;
    php_parallel_link_cond_t  c;
    php_parallel_link_state_t s;

    union {
        php_parallel_link_queue_t q;
        zval                      z;
    } port;

    uint32_t refcount;
};

static zend_string  *php_parallel_link_string_name,
                    *php_parallel_link_string_type,
                    *php_parallel_link_string_unbuffered,
                    *php_parallel_link_string_buffered,
                    *php_parallel_link_string_capacity,
                    *php_parallel_link_string_size,
                    *php_parallel_link_string_infinite;

static zend_always_inline int32_t php_parallel_link_mutex_init(php_parallel_link_mutex_t *mutex) {
    if (!php_parallel_mutex_init(&mutex->m, 1)) {
        return FAILURE;
    }

    if (!php_parallel_mutex_init(&mutex->r, 1)) {
        php_parallel_mutex_destroy(&mutex->m);

        return FAILURE;
    }

    if (!php_parallel_mutex_init(&mutex->w, 1)) {
        php_parallel_mutex_destroy(&mutex->m);
        php_parallel_mutex_destroy(&mutex->r);

        return FAILURE;
    }

    return SUCCESS;
}

static zend_always_inline void php_parallel_link_mutex_destroy(php_parallel_link_mutex_t *mutex) {
    php_parallel_mutex_destroy(&mutex->m);
    php_parallel_mutex_destroy(&mutex->r);
    php_parallel_mutex_destroy(&mutex->w);
}

static zend_always_inline int32_t php_parallel_link_cond_init(php_parallel_link_cond_t *condition) {
    if (!php_parallel_cond_init(&condition->r)) {
        return FAILURE;
    }

    if (!php_parallel_cond_init(&condition->w)) {
        php_parallel_cond_destroy(&condition->r);

        return FAILURE;
    }

    return SUCCESS;
}

static zend_always_inline void php_parallel_link_cond_destroy(php_parallel_link_cond_t *condition) {
    pthread_cond_destroy(&condition->r);
    pthread_cond_destroy(&condition->w);
}

php_parallel_link_t* php_parallel_link_init(zend_string *name, zend_bool buffered, zend_long capacity) {
    php_parallel_link_t *link = pecalloc(1, sizeof(php_parallel_link_t), 1);

    if (php_parallel_link_mutex_init(&link->m) != SUCCESS) {
        pefree(link, 1);
        return NULL;
    }

    if (php_parallel_link_cond_init(&link->c) != SUCCESS) {
        php_parallel_link_mutex_destroy(&link->m);
        pefree(link, 1);
        return NULL;
    }

    if (buffered) {
        link->type = PHP_PARALLEL_LINK_BUFFERED;
        zend_llist_init(&link->port.q.l,
            sizeof(zval),
            (llist_dtor_func_t) PARALLEL_ZVAL_DTOR, 1);
        link->port.q.c = capacity;
    } else {
        link->type = PHP_PARALLEL_LINK_UNBUFFERED;
    }
    link->name = php_parallel_copy_string_interned(name);
    link->refcount = 1;

    return link;
}

void php_parallel_link_destroy(php_parallel_link_t *link) {
    if (php_parallel_atomic_delref(&link->refcount) == 0) {
        php_parallel_link_mutex_destroy(&link->m);
        php_parallel_link_cond_destroy(&link->c);

        if (link->type == PHP_PARALLEL_LINK_BUFFERED) {
            zend_llist_destroy(&link->port.q.l);
        } else {
            if (Z_TYPE_FLAGS(link->port.z) == PHP_PARALLEL_LINK_CLOSURE_BUFFER) {
                PARALLEL_ZVAL_DTOR(&link->port.z);
            }
        }
        pefree(link, 1);
    }
}

php_parallel_link_t* php_parallel_link_copy(php_parallel_link_t *link) {
    php_parallel_atomic_addref(&link->refcount);

    return link;
}

static zend_always_inline zend_bool php_parallel_link_send_unbuffered(php_parallel_link_t *link, zval *value) {
    pthread_mutex_lock(&link->m.w);
    pthread_mutex_lock(&link->m.m);

    if (link->s.c) {
        pthread_mutex_unlock(&link->m.m);
        pthread_mutex_unlock(&link->m.w);
        return 0;
    }

    if (PARALLEL_ZVAL_CHECK_CLOSURES(value)) {
        PARALLEL_ZVAL_COPY(
            &link->port.z, value, 1);
        Z_TYPE_FLAGS(link->port.z) =
            PHP_PARALLEL_LINK_CLOSURE_BUFFER;
    } else {
        ZVAL_COPY_VALUE(&link->port.z, value);

        ZEND_ASSERT(
            Z_TYPE_FLAGS(link->port.z) != PHP_PARALLEL_LINK_CLOSURE_BUFFER);
    }
    link->s.w++;

    if (link->s.r) {
        pthread_cond_signal(&link->c.r);
    }

    pthread_cond_wait(&link->c.w, &link->m.m);

    pthread_mutex_unlock(&link->m.m);
    pthread_mutex_unlock(&link->m.w);

    return 1;
}

static zend_always_inline zend_bool php_parallel_link_send_buffered(php_parallel_link_t *link, zval *value) {
    zval sent;

    pthread_mutex_lock(&link->m.m);

    while (link->port.q.c &&
           zend_llist_count(&link->port.q.l) == link->port.q.c) {
        link->s.w++;
        pthread_cond_wait(&link->c.w, &link->m.m);
        link->s.w--;
    }

    if (link->s.c) {
        pthread_mutex_unlock(&link->m.m);
        return 0;
    }

    PARALLEL_ZVAL_COPY(&sent, value, 1);

    zend_llist_add_element(
        &link->port.q.l, &sent);

    if (link->s.r) {
        pthread_cond_signal(&link->c.r);
    }

    pthread_mutex_unlock(&link->m.m);
    return 1;
}

zend_bool php_parallel_link_send(php_parallel_link_t *link, zval *value) {
    if (link->type == PHP_PARALLEL_LINK_UNBUFFERED) {
        return php_parallel_link_send_unbuffered(link, value);
    } else {
        return php_parallel_link_send_buffered(link, value);
    }
}

static zend_always_inline zend_bool php_parallel_link_recv_unbuffered(php_parallel_link_t *link, zval *value) {
    pthread_mutex_lock(&link->m.r);
    pthread_mutex_lock(&link->m.m);

    while (!link->s.c && !link->s.w) {
        link->s.r++;
        pthread_cond_wait(&link->c.r, &link->m.m);
        link->s.r--;
    }

    if (link->s.c) {
        pthread_mutex_unlock(&link->m.m);
        pthread_mutex_unlock(&link->m.r);
        return 0;
    }

    PARALLEL_ZVAL_COPY(
        value, &link->port.z, 0);
    if (Z_TYPE_FLAGS(link->port.z) == PHP_PARALLEL_LINK_CLOSURE_BUFFER) {
        PARALLEL_ZVAL_DTOR(&link->port.z);
    }
    ZVAL_UNDEF(&link->port.z);
    link->s.w--;

    pthread_cond_signal(&link->c.w);
    pthread_mutex_unlock(&link->m.m);
    pthread_mutex_unlock(&link->m.r);
    return 1;
}

static zend_always_inline int php_parallel_link_queue_delete(void *lhs, void *rhs) {
    return lhs == rhs;
}

static zend_always_inline zend_bool php_parallel_link_recv_buffered(php_parallel_link_t *link, zval *value) {
    zval *head;

    pthread_mutex_lock(&link->m.m);

    while (zend_llist_count(&link->port.q.l) == 0) {
        if (link->s.c) {
            pthread_mutex_unlock(&link->m.m);
            return 0;
        }

        link->s.r++;
        pthread_cond_wait(&link->c.r, &link->m.m);
        link->s.r--;
    }

    head = zend_llist_get_first(&link->port.q.l);

    PARALLEL_ZVAL_COPY(value, head, 0);

    zend_llist_del_element(
        &link->port.q.l, head, php_parallel_link_queue_delete);

    if (link->s.w) {
        pthread_cond_signal(&link->c.w);
    }

    pthread_mutex_unlock(&link->m.m);

    return 1;
}

zend_bool php_parallel_link_recv(php_parallel_link_t *link, zval *value) {
    if (link->type == PHP_PARALLEL_LINK_UNBUFFERED) {
        return php_parallel_link_recv_unbuffered(link, value);
    } else {
        return php_parallel_link_recv_buffered(link, value);
    }
}

zend_bool php_parallel_link_close(php_parallel_link_t *link) {
    pthread_mutex_lock(&link->m.m);

    if (link->s.c) {
        pthread_mutex_unlock(&link->m.m);
        return 0;
    }

    link->s.c = 1;
    pthread_cond_broadcast(&link->c.r);
    pthread_cond_broadcast(&link->c.w);
    pthread_mutex_unlock(&link->m.m);

    return 1;
}

zend_bool php_parallel_link_closed(php_parallel_link_t *link) {
    return link->s.c;
}

zend_string* php_parallel_link_name(php_parallel_link_t *link) {
    return link->name;
}

zend_bool php_parallel_link_lock(php_parallel_link_t *link) {
    return pthread_mutex_lock(&link->m.m) == SUCCESS;
}

zend_bool php_parallel_link_writable(php_parallel_link_t *link) {
    switch (link->type) {
        case PHP_PARALLEL_LINK_UNBUFFERED:
            return link->s.r > 0;

        case PHP_PARALLEL_LINK_BUFFERED:
            return zend_llist_count(&link->port.q.l) < link->port.q.c;
    }

    return 0;
}

zend_bool php_parallel_link_readable(php_parallel_link_t *link) {
    switch (link->type) {
        case PHP_PARALLEL_LINK_UNBUFFERED:
            return link->s.w > 0;

        case PHP_PARALLEL_LINK_BUFFERED:
            return zend_llist_count(&link->port.q.l) > 0;
    }

    return 0;
}

void php_parallel_link_debug(php_parallel_link_t *link, HashTable *debug) {
    zval zdbg;

    ZVAL_STR(&zdbg, link->name);

    zend_hash_add(debug, php_parallel_link_string_name, &zdbg);

    switch (link->type) {
        case PHP_PARALLEL_LINK_UNBUFFERED:
            ZVAL_STR_COPY(&zdbg,
                php_parallel_link_string_unbuffered);
            zend_hash_add(debug, php_parallel_link_string_type, &zdbg);
        break;

        case PHP_PARALLEL_LINK_BUFFERED:
            ZVAL_STR_COPY(&zdbg,
                php_parallel_link_string_buffered);
            zend_hash_add(debug, php_parallel_link_string_type, &zdbg);

            if (link->port.q.c == -1) {
                ZVAL_STR_COPY(&zdbg,
                    php_parallel_link_string_infinite);
                zend_hash_add(
                    debug,
                    php_parallel_link_string_capacity, &zdbg);
            } else {
                ZVAL_LONG(&zdbg, link->port.q.c);
                zend_hash_add(
                    debug,
                    php_parallel_link_string_capacity, &zdbg);
                if (link->port.q.l.count) {
                    ZVAL_LONG(&zdbg, link->port.q.l.count);
                    zend_hash_add(
                        debug,
                        php_parallel_link_string_size, &zdbg);
                }
            }
        break;
    }
}

zend_bool php_parallel_link_unlock(php_parallel_link_t *link) {
    return pthread_mutex_unlock(&link->m.m) == SUCCESS;
}

PHP_MINIT_FUNCTION(PARALLEL_LINK)
{
    php_parallel_link_string_name       = zend_string_init_interned(ZEND_STRL("name"), 1);
    php_parallel_link_string_type       = zend_string_init_interned(ZEND_STRL("type"), 1);
    php_parallel_link_string_unbuffered = zend_string_init_interned(ZEND_STRL("unbuffered"), 1);
    php_parallel_link_string_buffered   = zend_string_init_interned(ZEND_STRL("buffered"), 1);
    php_parallel_link_string_capacity   = zend_string_init_interned(ZEND_STRL("capacity"), 1);
    php_parallel_link_string_size       = zend_string_init_interned(ZEND_STRL("size"), 1);
    php_parallel_link_string_infinite   = zend_string_init_interned(ZEND_STRL("infinite"), 1);

    return SUCCESS;
}

PHP_MSHUTDOWN_FUNCTION(PARALLEL_LINK)
{
    zend_string_release(php_parallel_link_string_name);
    zend_string_release(php_parallel_link_string_type);
    zend_string_release(php_parallel_link_string_unbuffered);
    zend_string_release(php_parallel_link_string_buffered);
    zend_string_release(php_parallel_link_string_capacity);
    zend_string_release(php_parallel_link_string_size);
    zend_string_release(php_parallel_link_string_infinite);

    return SUCCESS;
}
#endif
