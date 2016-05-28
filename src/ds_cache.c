#include "nfs41_client.h"
#include "list.h"
#include <pthread.h>

/* nfs41_server_list */
struct server_list {
    struct list_entry head;
    pthread_mutex_t  lock; 
};


static struct server_list g_server_list;

#define server_entry(pos) list_container(pos, rpc_client_t, entry)


void nfs41_server_list_init()
{
    list_init(&g_server_list.head);
    pthread_mutex_init(&g_server_list.lock, NULL);
}

static int server_compare(
    const struct list_entry *entry,
    const void *value)
{
    const rpc_client_t *server = server_entry(entry);
    const netaddr4 *addr = (const struct netaddr4*)value;
    const int diff = strcmp(server->addr->na_r_addr, addr->na_r_addr);
    return diff ? diff : strcmp(server->addr->na_r_netid, addr->na_r_netid);
}


static int server_entry_find(
    struct server_list *servers,
    const struct netaddr4 *addr,
    struct list_entry **entry_out)
{
    *entry_out = list_search(&servers->head, addr, server_compare);
    return *entry_out ? 0 : -1;
}

int find_or_create(netaddr4 *addr, rpc_client_t **client_out)
{
    struct list_entry *entry;
    rpc_client_t *client;
    int status;

    pthread_mutex_lock(&g_server_list.lock);

    status = server_entry_find(&g_server_list, addr, &entry);
    if(status == 0) {
        client = server_entry(entry);        
    }else{
        client = get_client(addr);
        if(client != NULL) {
            list_add_tail(&g_server_list.head, &client->entry);
            status = 0;
        }
    }

    pthread_mutex_unlock(&g_server_list.lock);

    *client_out = client;
    return status;
}

rpc_client_t *
get_client(netaddr4 *netaddr)
{
    
    rpc_client_t *rpc_client;
    struct netconfig *nconf;
    struct netbuf *addr;

    rpc_client = malloc(sizeof (rpc_client_t));
    if (rpc_client == NULL)
        goto err1;

    nconf = getnetconfigent(netaddr->na_r_netid);
    if (nconf == NULL)  goto err2;

    addr = uaddr2taddr(nconf, netaddr->na_r_addr);    
    if (addr == NULL) goto err3;

    rpc_client->client = clnt_tli_create(RPC_ANYFD, nconf, addr,
        NFS4_PROGRAM, NFS_V4, 0, 0);

    freenetconfigent(nconf);
    free(addr);

    if (rpc_client->client == NULL)
        goto err2;

    rpc_client->addr = malloc(sizeof(netaddr4));
    if(rpc_client->addr == NULL)
        goto err3;

    rpc_client->addr->na_r_addr = strdup(netaddr->na_r_addr);
    if(rpc_client->addr->na_r_addr == NULL)
        goto err4;

    rpc_client->addr->na_r_netid = strdup(netaddr->na_r_netid);
    if (rpc_client->addr->na_r_netid == NULL)
        goto err5;

    pthread_mutex_init(&rpc_client->lock, NULL);

    return rpc_client;

err5:
    free(rpc_client->addr->na_r_addr);
err4:
    free(rpc_client->addr);
err3:
    freenetconfigent(nconf);
err2:
    free(rpc_client);
    return NULL;
err1:
    return NULL;
}
