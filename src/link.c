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

typedef struct _php_parallel_link_t {
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

static zend_always_inline int32_t php_parallel_link_mutex_init(php_parallel_link_mutex_t *mutex) {
    pthread_mutexattr_t attributes;

    pthread_mutexattr_init(&attributes);

#if defined(PTHREAD_MUTEX_RECURSIVE) || defined(__FreeBSD__)
     pthread_mutexattr_settype(&attributes, PTHREAD_MUTEX_RECURSIVE);
#else
     pthread_mutexattr_settype(&attributes, PTHREAD_MUTEX_RECURSIVE_NP);
#endif

    if (pthread_mutex_init(&mutex->m, &attributes) != SUCCESS) {
        return FAILURE;
    }

    if (pthread_mutex_init(&mutex->r, &attributes) != SUCCESS) {
        pthread_mutex_destroy(&mutex->m);

        return FAILURE;
    }

    if (pthread_mutex_init(&mutex->w, &attributes) != SUCCESS) {
        pthread_mutex_destroy(&mutex->m);
        pthread_mutex_destroy(&mutex->r);

        return FAILURE;
    }

    return SUCCESS;
}

static zend_always_inline void php_parallel_link_mutex_destroy(php_parallel_link_mutex_t *mutex) {
    pthread_mutex_destroy(&mutex->m);
    pthread_mutex_destroy(&mutex->r);
    pthread_mutex_destroy(&mutex->w);
}

static zend_always_inline int32_t php_parallel_link_cond_init(php_parallel_link_cond_t *condition) {
    if (pthread_cond_init(&condition->r, NULL) != SUCCESS) {
        return FAILURE;
    }

    if (pthread_cond_init(&condition->w, NULL) != SUCCESS) {
        pthread_cond_destroy(&condition->r);

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
    link->name = zend_string_init(
        ZSTR_VAL(name), ZSTR_LEN(name), 1);
    link->refcount = 1;

    return link;
}

void php_parallel_link_destroy(php_parallel_link_t *link) {
    pthread_mutex_lock(&link->m.m);

    if (--link->refcount == 0) {
        pthread_mutex_unlock(&link->m.m);

        php_parallel_link_mutex_destroy(&link->m);
        php_parallel_link_cond_destroy(&link->c);

        if (link->type == PHP_PARALLEL_LINK_BUFFERED) {
            zend_llist_destroy(&link->port.q.l);
        } else {
            if (PARALLEL_ZVAL_CHECK_CLOSURES(&link->port.z)) {
                PARALLEL_ZVAL_DTOR(&link->port.z);
            }
        }
        zend_string_release(link->name);
        pefree(link, 1);
    } else {
        pthread_mutex_unlock(&link->m.m);
    }
}

php_parallel_link_t* php_parallel_link_copy(php_parallel_link_t *link) {
    pthread_mutex_lock(&link->m.m);

    link->refcount++;

    pthread_mutex_unlock(&link->m.m);

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
        PARALLEL_ZVAL_COPY(&link->port.z, value, 1);
    } else {
        ZVAL_COPY_VALUE(&link->port.z, value);
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
    if (PARALLEL_ZVAL_CHECK_CLOSURES(&link->port.z)) {
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

    if (link->s.c) {
        pthread_mutex_unlock(&link->m.m);
        return 0;
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

zend_bool php_parallel_link_unlock(php_parallel_link_t *link) {
    return pthread_mutex_unlock(&link->m.m) == SUCCESS;
}
#endif
