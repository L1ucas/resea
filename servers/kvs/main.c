#include <list.h>
#include <message.h>
#include <std/async.h>
#include <std/malloc.h>
#include <std/printf.h>
#include <std/syscall.h>
#include <string.h>
#include "kvs.h"

static list_t entries;

/// Search the database for the key. Note that `key` can NOT be terminated by
/// null.
static struct entry *get(const char *key) {
    LIST_FOR_EACH (e, &entries, struct entry, next) {
        if (!strncmp(e->key, key, KVS_KEY_LEN)) {
            return e;
        }
    }

    return NULL;
}

/// Creates a new entry with the given key. Note that `key` can NOT be
/// terminated by null.
static struct entry *create(const char *key) {
    ASSERT(get(key) == NULL);
    struct entry *e = malloc(sizeof(*e));
    e->len = 0;
    strncpy(e->key, key, sizeof(e->key));
    list_init(&e->listeners);
    list_push_back(&entries, &e->next);
    return e;
}

static void update(struct entry *e, const void *data, size_t len) {
    ASSERT(len <= KVS_DATA_LEN_MAX);
    memcpy(e->data, data, len);
    e->len = len;
}

static void delete(struct entry *e) {
    list_remove(&e->next);
    free(e);
}

static void listen(struct entry *e, tid_t task) {
    struct listener *listener = malloc(sizeof(*listener));
    listener->task = task;
    list_push_back(&e->listeners, &listener->next);
}

static void unlisten(struct entry *e, tid_t task) {
    LIST_FOR_EACH (listener, &e->listeners, struct listener, next) {
        if (listener->task == task) {
            list_remove(&listener->next);
        }
    }
}

static void changed(struct entry *e) {
    struct message m;
    m.type = KVS_CHANGED_MSG;
    strncpy(m.kvs.changed.key, e->key, sizeof(m.kvs.changed.key));
    LIST_FOR_EACH (listener, &e->listeners, struct listener, next) {
        async_send(listener->task, &m);
    }
}

static void deferred_work(void) {
    async_flush();
}

void main(void) {
    INFO("starting...");
    list_init(&entries);

    INFO("ready");
    while (true) {
        struct message m;
        error_t err = ipc_recv(IPC_ANY, &m);
        ASSERT_OK(err);

        switch (m.type) {
            case NOTIFICATIONS_MSG:
                break;
            case KVS_GET_MSG: {
                struct entry *e = get(m.kvs.get.key);
                if (!e) {
                    ipc_reply_err(m.src, ERR_NOT_FOUND);
                    break;
                }

                TRACE("GET '%s' (len=%d)", e->key, e->len);
                ASSERT(e->len <= KVS_DATA_LEN_MAX);
                m.type = KVS_GET_REPLY_MSG;
                m.kvs.get_reply.len = e->len;
                memcpy(m.kvs.get_reply.data, e->data, e->len);
                memset(&m.kvs.get_reply.data[e->len], 0,
                       KVS_DATA_LEN_MAX - e->len);
                ipc_reply(m.src, &m);
                break;
            }
            case KVS_SET_MSG: {
                if (m.kvs.set.len > KVS_DATA_LEN_MAX) {
                    ipc_reply_err(m.src, ERR_TOO_LARGE);
                    break;
                }

                struct entry *e = get(m.kvs.set.key);
                if (!e) {
                    e = create(m.kvs.set.key);
                }

                TRACE("SET '%s' (len=%d)", e->key, m.kvs.set.len);
                update(e, m.kvs.set.data, m.kvs.set.len);
                ipc_reply_err(m.src, OK);
                changed(e);
                break;
            }
            case KVS_DELETE_MSG: {
                struct entry *e = get(m.kvs.delete.key);
                if (!e) {
                    ipc_reply_err(m.src, ERR_NOT_FOUND);
                    break;
                }

                TRACE("DELETE '%s'", e->key);
                delete (e);
                ipc_reply_err(m.src, OK);
                break;
            }
            case KVS_LISTEN_MSG: {
                struct entry *e = get(m.kvs.listen.key);
                if (!e) {
                    ipc_reply_err(m.src, ERR_NOT_FOUND);
                    break;
                }

                TRACE("LISTEN '%s' (task=%d)", e->key, m.src);
                listen(e, m.src);
                ipc_reply_err(m.src, OK);
                break;
            }
            case KVS_UNLISTEN_MSG: {
                struct entry *e = get(m.kvs.unlisten.key);
                if (!e) {
                    ipc_reply_err(m.src, ERR_NOT_FOUND);
                    break;
                }

                TRACE("UNLISTEN '%s' (task=%d)", e->key, m.src);
                unlisten(e, m.src);
                ipc_reply_err(m.src, OK);
                break;
            }
            default:
                TRACE("unknown message %d", m.type);
        }

        deferred_work();
    }
}
