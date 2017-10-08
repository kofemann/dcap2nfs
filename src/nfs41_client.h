/*
 * File:   nfs41_client.h
 * Author: tigran
 *
 * Created on September 24, 2010, 9:35 AM
 */

#ifndef NFS41_CLIENT_H
#define	NFS41_CLIENT_H

#include <rpc/rpc.h>
#include <pthread.h>
#include "nfs4_prot.h"

#ifdef	__cplusplus
extern "C" {
#endif

#define IID_NAME  "dcap built-in NFSv4.1 client " __DATE__ " " __TIME__
#define IID_DOMAIN "dCache.ORG"

/* doubly-linked list */
struct list_entry {
    struct list_entry *prev;
    struct list_entry *next;
};

typedef struct {
    CLIENT* client;
    netaddr4 *addr;
    struct list_entry entry;
    pthread_mutex_t lock;
} rpc_client_t;

typedef struct {
    pthread_mutex_t lock;
    char *principal;
    client_owner4 client_owner;
    clientid4 clientid;
    sequenceid4 sequenceid;
    uint32_t server_flags;
    rpc_client_t *rpc_client;
    uint32_t wsize;
    uint32_t rsize;
    netaddr4 *server_addr;
} nfs_client_t;

typedef struct {
    server_owner4 server_owner;
     struct {
       u_int server_scope_len;
       char *server_scope_val;
    } server_scop;
    nfs_impl_id4 iid;
} nfs_server_t;

#define MAX_SLOT 16
typedef struct {
    pthread_mutex_t lock;
    pthread_cond_t slot_free;
    uint32_t seq[MAX_SLOT];
    uint32_t used[MAX_SLOT];
    uint32_t last_used;
} slot_table_t;

typedef struct {
    nfs_client_t *client;
    unsigned char session_id[NFS4_SESSIONID_SIZE];
    channel_attrs4 fore_chan_attrs;
    channel_attrs4 back_chan_attrs;
    slot_table_t slot_table;
    uint32_t lease_time;
    uint32_t flags;
    bool_t is_valid;
    pthread_t sequence_thread;
} nfs_session_t;

rpc_client_t* get_client(netaddr4 *addr);
#ifdef	__cplusplus
}
#endif

#endif	/* NFS41_CLIENT_H */
