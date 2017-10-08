/*
 * File:   main.c
 * Author: tigran
 *
 * Created on September 24, 2010, 3:49 PM
 */

#include <stdio.h>
#include <stdlib.h>
#include "nfs41_client.h"
#include "nfsv41_ops.h"

/*
 *
 */
int main(int argc, char** argv) {

    nfs_client_t nfs_client;
    nfs_session_t session;

    if (argc != 3) {
        printf("Usage: %s <host> <port>\n", argv[0]);
        exit(1);
    }

    nfs_client.wsize = 23*1024;
    nfs_client.rsize = 23*1024;
    pthread_mutex_init(&nfs_client.lock, NULL);

    nfs41_server_list_init();
    nfs_client.server_addr = name_to_netaddr(argv[1], atoi(argv[2]));
    printf("netaddr4: %s://%s\n", nfs_client.server_addr->na_r_netid, nfs_client.server_addr->na_r_addr);

    exchange_id(&nfs_client, 0);
    printf("client id: 0x%lx\n", nfs_client.clientid);

    session.client = &nfs_client;
    session.flags = 0;
    session.client->sequenceid = 1;

    create_session(&session);

    pthread_join(session.sequence_thread, NULL);
    destroy_session(&session);
    return (EXIT_SUCCESS);
}
