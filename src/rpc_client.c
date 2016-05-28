#include <rpc/rpc.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include "nfs41_client.h"

struct sockaddr_in*
name_to_sockaddr(const char* host, short port) {

    struct sockaddr_in* serv_addr;
    struct hostent *hp;

    serv_addr = malloc(sizeof (struct sockaddr_in));
    if (serv_addr == NULL)
        goto err1;

    memset((char *)serv_addr, 0, sizeof (serv_addr));

    serv_addr->sin_family = AF_INET;
    serv_addr->sin_port = htons(port);

    /* first try  by host name, then by address */
    hp = (struct hostent *) gethostbyname(host);
    if (hp == NULL) 
        goto err2;

    memcpy(&serv_addr->sin_addr.s_addr, hp->h_addr_list[0], hp->h_length);
    return serv_addr;

err2:
    free(serv_addr);
err1:
    return NULL;
}

netaddr4*
name_to_netaddr(const char* host, short port) {

    struct addrinfo hints, *res;
    char service[16];
    netaddr4 *netaddr;
    struct netbuf addr;
    struct netconfig *netconfig;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    sprintf(service , "%d", port);
    if (getaddrinfo(host, service, &hints, &res) != 0) goto err1;

    if( res == NULL) goto err1;
    
    netaddr = malloc(sizeof (netaddr4));
    if (netaddr == NULL)
        goto err1;

    switch(res->ai_family ) {
        case AF_INET: netaddr->na_r_netid = strdup("tcp"); break;
        case AF_INET6: netaddr->na_r_netid = strdup("tcp6"); break;
        default: goto err2;
    }

    netconfig = getnetconfigent(netaddr->na_r_netid);
    if(netconfig == NULL) goto err2;

    addr.maxlen = res->ai_addrlen;
    addr.len = res->ai_addrlen;
    addr.buf = res->ai_addr;

    netaddr->na_r_addr = taddr2uaddr(netconfig, &addr);

    freenetconfigent(netconfig);

    if(netaddr->na_r_addr == NULL) goto err2;

    return netaddr;
    
err2:
    free(netaddr);
err1:
    return NULL;
}
