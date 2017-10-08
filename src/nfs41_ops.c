#include <sys/time.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <rpc/rpc.h>
#include <rpc/clnt.h>
#include <pthread.h>
#include "nfs4_prot.h"
#include "nfs41_client.h"
#include "utf8_utils.h"
#include "nfsv41_ops.h"
#include "compound.h"

static struct timeval TIMEOUT = {30,0};

nfs_impl_id4
get_impl_id() {
    nfs_impl_id4 iid;

    char_to_utf8str_cs(IID_DOMAIN, &iid.nii_domain);
    char_to_utf8str_cs(IID_NAME, &iid.nii_name);
    iid.nii_date.seconds = 0;
    iid.nii_date.nseconds = 0;
    return iid;
}


#define MAC_LEN 6

int
get_mac_address(unsigned char *data_ptr) {

    struct ifreq ifr;
    struct sockaddr *sa;
    int s;
    int i;

    if ((s = socket(PF_INET, SOCK_DGRAM, 0)) < 0)
        return -1;
    sprintf(ifr.ifr_name, "eth0");

    if (ioctl(s, SIOCGIFHWADDR, &ifr) < 0) {
        close(s);
        return -1;
    }
    sa = (struct sockaddr *) &ifr.ifr_addr;
    for (i = 0; i < MAC_LEN; i++)
        data_ptr[i] = (unsigned char) (sa->sa_data[i] & 0xff);
    close(s);

    return 0;

}

/*
 * Use host mac address, uid and gid to generate client id
 */
int init_clinet_owner(client_owner4 *client_owner)
{
    time_t creation_time;
    pid_t pid;
    uid_t uid;

    creation_time = time(NULL);
    pid = getpid();
    uid = getuid();

    memset(client_owner->co_verifier, 0, sizeof(client_owner->co_verifier));
    memcpy(client_owner->co_verifier, &creation_time, sizeof(creation_time));

    /*
     * FIXME: use something else
     */

    client_owner->co_ownerid.co_ownerid_len = MAC_LEN + sizeof(pid_t) + sizeof(uid_t);
    client_owner->co_ownerid.co_ownerid_val = malloc(client_owner->co_ownerid.co_ownerid_len);
    get_mac_address((unsigned char *)client_owner->co_ownerid.co_ownerid_val);
    memcpy(client_owner->co_ownerid.co_ownerid_val + MAC_LEN, &pid, sizeof(pid_t));
    memcpy(client_owner->co_ownerid.co_ownerid_val + MAC_LEN + sizeof(pid_t), &uid, sizeof(uid_t));
    return 0;
}

int init_server(nfs_server_t *server) {

    memset(server, 0, sizeof(nfs_server_t));
    return 0;
}

int
send_compound(rpc_client_t *client, COMPOUND4args* args, COMPOUND4res* res)
{
    int rc = 0;
    enum clnt_stat status;

    memset(res, 0, sizeof(COMPOUND4res));

    pthread_mutex_lock(&client->lock);

    status = clnt_call(client->client, NFSPROC4_COMPOUND,
            (xdrproc_t) xdr_COMPOUND4args, (caddr_t) args,
            (xdrproc_t) xdr_COMPOUND4res, (caddr_t) res,
            TIMEOUT);

    if( status != RPC_SUCCESS)
        rc = -1;

    pthread_mutex_unlock(&client->lock);

    return rc;
}

int
send_null(rpc_client_t *client)
{
    int rc = 0;
    enum clnt_stat status;

    pthread_mutex_lock(&client->lock);

    status = clnt_call(client->client, NFSPROC4_NULL,
            (xdrproc_t) xdr_void, (caddr_t) NULL,
            (xdrproc_t) xdr_void, (caddr_t) NULL,
            TIMEOUT);

    if( status != RPC_SUCCESS)
        rc = -1;

    pthread_mutex_unlock(&client->lock);

    return rc;
}

int
free_compound(COMPOUND4res* res) {
    xdr_free((xdrproc_t) xdr_COMPOUND4res, (caddr_t) res);
    return 0;
}

int
exchange_id(nfs_client_t* client, int flags)
{

    COMPOUND4args c_args;
    COMPOUND4res c_res;
    nfs_argop4 op;
    nfs_impl_id4 iid;
    int rc = -1;
    nfs_server_t server;
    rpc_client_t *rpc_client;

    pthread_mutex_lock(&client->lock);

    init_clinet_owner(&op.nfs_argop4_u.opexchange_id.eia_clientowner);
    op.argop = OP_EXCHANGE_ID;
    op.nfs_argop4_u.opexchange_id.eia_flags = flags;

    iid = get_impl_id();
    op.nfs_argop4_u.opexchange_id.eia_client_impl_id.eia_client_impl_id_len = 1;
    op.nfs_argop4_u.opexchange_id.eia_client_impl_id.eia_client_impl_id_val = &iid;
    op.nfs_argop4_u.opexchange_id.eia_state_protect.spa_how = SP4_NONE;


    c_args.minorversion = 1;
    c_args.argarray.argarray_val = &op;
    c_args.argarray.argarray_len = 1;
    char_to_utf8str_cs("exchange_id", &c_args.tag);

    init_server(&server);

    /*
     * FIXME:
     */
    find_or_create(client->server_addr, &rpc_client);
    if (send_compound(rpc_client, &c_args, &c_res) != 0)
        goto clean_rpc;

    client->clientid = c_res.resarray.resarray_val[0].nfs_resop4_u.opexchange_id.EXCHANGE_ID4res_u.eir_resok4.eir_clientid;

    rc = 0;
clean_rpc:
    free_compound(&c_res);
    pthread_mutex_unlock(&client->lock);
    return rc;
}

pthread_cond_t fakeCond = PTHREAD_COND_INITIALIZER;

void * sequence_thread(void *arg) {
    nfs_session_t *session = (nfs_session_t *) arg;

    struct timespec timeToWait;
    struct timeval now;

    while (1) {
        pthread_mutex_lock(&session->client->lock);
        if (!session->is_valid) {
            pthread_mutex_unlock(&session->client->lock);
            break;
        }
        sequence(session);

        gettimeofday(&now, NULL);

        timeToWait.tv_sec = now.tv_sec + 5;
        timeToWait.tv_nsec = now.tv_usec * 1000;

        pthread_cond_timedwait(&fakeCond, &session->client->lock, &timeToWait);
        pthread_mutex_unlock(&session->client->lock);
    };

    return NULL;
}

static int set_fore_channel_attrs(
    nfs_client_t *rpc,
    uint32_t max_req,
    channel_attrs4 *attrs)
{
    attrs->ca_headerpadsize = 0;
    attrs->ca_maxrequestsize = rpc->wsize;
    attrs->ca_maxresponsesize = rpc->rsize;
    attrs->ca_maxresponsesize_cached = 1024;
    attrs->ca_maxoperations = 0xffffffff;
    attrs->ca_maxrequests = max_req;
    attrs->ca_rdma_ird.ca_rdma_ird_len = 0;
    return 0;
}

static int set_back_channel_attrs(
    nfs_client_t *rpc,
    uint32_t max_req,
    channel_attrs4 *attrs)
{
    attrs->ca_headerpadsize = 0;
    attrs->ca_maxrequestsize = rpc->wsize;
    attrs->ca_maxresponsesize = rpc->rsize;
    attrs->ca_maxresponsesize_cached = 0;
    attrs->ca_maxoperations = 0xffffffff;
    attrs->ca_maxrequests = max_req;
    attrs->ca_rdma_ird.ca_rdma_ird_len = 0;
    return 0;
}

int
create_session(nfs_session_t *session)
{
    COMPOUND4args c_args;
    COMPOUND4res c_res;
    nfs_argop4 op;
    rpc_client_t *rpc_client;
    CREATE_SESSION4resok *cs_res;

    op.argop = OP_CREATE_SESSION;
    op.nfs_argop4_u.opcreate_session.csa_clientid = session->client->clientid;
    op.nfs_argop4_u.opcreate_session.csa_sequence = session->client->sequenceid;
    op.nfs_argop4_u.opcreate_session.csa_flags = session->flags;
    set_back_channel_attrs(session->client, 128, &op.nfs_argop4_u.opcreate_session.csa_back_chan_attrs);
    set_fore_channel_attrs(session->client, 128, &op.nfs_argop4_u.opcreate_session.csa_fore_chan_attrs);
    op.nfs_argop4_u.opcreate_session.csa_sec_parms.csa_sec_parms_len = 0;

    c_args.minorversion = 1;
    c_args.argarray.argarray_val = &op;
    c_args.argarray.argarray_len = 1;
    char_to_utf8str_cs("create_session", &c_args.tag);

    find_or_create(session->client->server_addr, &rpc_client);
    if (send_compound(rpc_client, &c_args, &c_res) != 0)
        goto err1;

    cs_res = &c_res.resarray.resarray_val[0].nfs_resop4_u.opcreate_session.CREATE_SESSION4res_u.csr_resok4;
    memcpy(session->session_id, cs_res->csr_sessionid, NFS4_SESSIONID_SIZE);
    memset(&session->slot_table.used, 0, sizeof(session->slot_table.used));
    memset(&session->slot_table.seq, 0, sizeof(session->slot_table.seq));
    session->client->sequenceid++;
    session->is_valid = 1;
    pthread_create(&session->sequence_thread, NULL, sequence_thread, session);
    return 0;
 err1:
    return -1;
}


/**
* Find next available session slot. Block if required.
*/
int
find_session_slot(nfs_session_t *session)
{
    int i;
    int rc;
    int found = 0;
    pthread_mutex_lock(&session->slot_table.lock);
    do {
        for(i = 0; i < MAX_SLOT; i++) {
            if (!session->slot_table.used[i]) {
                found = 1;
                goto out;
            } else {
                printf("0x%lx: Slot in use: %d %d %d\n", session->client->clientid, i,
                    session->slot_table.used[i], session->slot_table.seq[i]);
            }
        }
        printf("0x%lx: All slots busy. Wating...\n", session->client->clientid);
        rc = pthread_cond_wait(&session->slot_table.slot_free, &session->slot_table.lock);
        if (rc) {
            perror("cond wait");
        }
    } while(!found);

out:
    printf("0x%lx: Using slot %d\n", session->client->clientid, i);
    session->slot_table.used[i] = 1;
    pthread_mutex_unlock(&session->slot_table.lock);
    return i;
}

int
release_session_slot(nfs_session_t *session, int slot)
{
    printf("0x%lx: Releasing slot %d\n", session->client->clientid, slot);
    pthread_mutex_lock(&session->slot_table.lock);
    pthread_cond_broadcast(&session->slot_table.slot_free);
    session->slot_table.used[slot] = 0;
    pthread_mutex_unlock(&session->slot_table.lock);
    return 0;
}


int
sequence(nfs_session_t *session)
{
    COMPOUND4args c_args;
    COMPOUND4res c_res;

    nfs_argop4 op;
    rpc_client_t *rpc_client;
    SEQUENCE4resok *cs_res;
    int session_slot;

    // find slot id to use
    session_slot = find_session_slot(session);

    op.argop = OP_SEQUENCE;
    op.nfs_argop4_u.opsequence.sa_slotid = session_slot;
    op.nfs_argop4_u.opsequence.sa_highest_slotid = MAX_SLOT - 1;
    op.nfs_argop4_u.opsequence.sa_cachethis = 1;
    op.nfs_argop4_u.opsequence.sa_sequenceid = session->slot_table.seq[session_slot];
    memcpy(op.nfs_argop4_u.opsequence.sa_sessionid, session->session_id, NFS4_SESSIONID_SIZE);

    compound_init(&c_args, &c_res, &op, 1, "sequence");

    find_or_create(session->client->server_addr, &rpc_client);
    if (send_compound(rpc_client, &c_args, &c_res) != 0)
        goto err1;
    /*
     * FIXME:
     */
    session->slot_table.seq[session_slot]++;
    release_session_slot(session, session_slot);
    cs_res = &c_res.resarray.resarray_val[0].nfs_resop4_u.opsequence.SEQUENCE4res_u.sr_resok4;
    return 0;
 err1:
    return -1;
}

int
destroy_session(nfs_session_t *session)
{
    COMPOUND4args c_args;
    COMPOUND4res c_res;
    nfs_argop4 op;
    rpc_client_t *rpc_client;

    op.argop = OP_DESTROY_SESSION;
    memcpy(op.nfs_argop4_u.opdestroy_session.dsa_sessionid, session->session_id, NFS4_SESSIONID_SIZE);

    c_args.minorversion = 1;
    c_args.argarray.argarray_val = &op;
    c_args.argarray.argarray_len = 1;
    char_to_utf8str_cs("destroy_session", &c_args.tag);

    find_or_create(session->client->server_addr, &rpc_client);
    if (send_compound(rpc_client, &c_args, &c_res) != 0)
        goto err1;

    session->is_valid = 0;
    return 0;
err1:
    return -1;

}

int
nfs_write(nfs_session_t *session, nfs_fh4 fh, stateid4 stateid,
    char *data, offset4 offset, length4 length)
{
    COMPOUND4args c_args;
    COMPOUND4res c_res;
    nfs_argop4 op[3];

    int session_slot;

    // find slot id to use
    session_slot = find_session_slot(session);

    op[0].argop = OP_SEQUENCE;
    op[0].nfs_argop4_u.opsequence.sa_slotid = session_slot;
    op[0].nfs_argop4_u.opsequence.sa_sequenceid = session->slot_table.seq[session_slot];
    op[0].nfs_argop4_u.opsequence.sa_cachethis = 0;
    memcpy(op[0].nfs_argop4_u.opsequence.sa_sessionid, session->session_id, NFS4_SESSIONID_SIZE);

    op[1].argop = OP_PUTFH;
    op[1].nfs_argop4_u.opputfh.object.nfs_fh4_len = fh.nfs_fh4_len;
    op[1].nfs_argop4_u.opputfh.object.nfs_fh4_val = fh.nfs_fh4_val;

    op[2].argop = OP_WRITE;
    op[2].nfs_argop4_u.opwrite.data.data_val = data;
    op[2].nfs_argop4_u.opwrite.data.data_len = length;
    op[2].nfs_argop4_u.opwrite.offset = offset;
    op[2].nfs_argop4_u.opwrite.stable = FILE_SYNC4;
    op[2].nfs_argop4_u.opwrite.stateid.seqid = stateid.seqid;
    memcpy(op[1].nfs_argop4_u.opwrite.stateid.other, stateid.other, sizeof(stateid.other));

    compound_init(&c_args, &c_res, op, 3, "write");


    release_session_slot(session, session_slot);
    return 0;
}
