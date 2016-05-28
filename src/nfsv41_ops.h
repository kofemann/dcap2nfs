/* 
 * File:   nfsv41_ops.h
 * Author: tigran
 *
 * Created on September 24, 2010, 4:51 PM
 */

#ifndef NFSV41_OPS_H
#define	NFSV41_OPS_H

#include "nfs4_prot.h"
#include "nfs41_client.h"

#ifdef	__cplusplus
extern "C" {
#endif

netaddr4* name_to_netaddr(const char* host, short port);
int init_clinet_owner(client_owner4 *client_owner);
int send_compound(rpc_client_t *client, COMPOUND4args* args, COMPOUND4res* res);
int exchange_id(nfs_client_t* client, int flags);
void nfs41_server_list_init();
int find_or_create(netaddr4 *addr, rpc_client_t **client_out);
int create_session(nfs_session_t *session);
int sequence(nfs_session_t *session);
int destroy_session(nfs_session_t *session);

#ifdef	__cplusplus
}
#endif

#endif	/* NFSV41_OPS_H */

