/**
 * UDP server that handles simple storage-lookup requests
 * 
 * CMPT 434 - A1
 * Steven Rau
 * scr108
 * 11115094
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include "storage.h"
#include "server.h"

/*-----------------------------------------------------------------------------
 * File-scope constants & globals
 * --------------------------------------------------------------------------*/

#define BUF_SIZE  1024

/* The storage "dictionary" array held in storage.c */
extern struct key_val_pair storage[MAX_STORAGE_SIZE];

/*-----------------------------------------------------------------------------
 * Helper functions
 * --------------------------------------------------------------------------*/

/**
 * Get sockaddr, IPv4 or IPv6:
 */
void *get_in_addr(struct sockaddr *sa)
{
    /* IPv4 case */
    if (sa->sa_family == AF_INET)
    {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    /* IPv6 case */
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

/*-----------------------------------------------------------------------------
 *
 * --------------------------------------------------------------------------*/

int main(int argc, char *argv[])
{
    uint32_t port_num;
    int sock_fd;
    struct addrinfo hints;
    struct addrinfo *serv_info;
    struct addrinfo *p;
    int rv;
    int num_bytes;
    struct sockaddr_storage their_addr;
    char buf[BUF_SIZE];
    char *reply_msg;
    socklen_t addr_len;
    char s[INET6_ADDRSTRLEN];
    
    if (argc < 2)
    {
        fprintf(stderr, "Usage: %s <port_number>\n", argv[0]);
        
        exit(1);
    }
    
    /* Grab the port number */
    port_num = atoi(argv[1]);
    if (port_num < MIN_PORT_NUM || port_num > MAX_PORT_NUM)
    {
        fprintf(stderr, "Usage: Port number must be between %d and %d\n", 
                         MIN_PORT_NUM, MAX_PORT_NUM);
        
        exit(1);
    }
    
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET6;      /* Use IPv6 */ 
    hints.ai_socktype = SOCK_DGRAM;  /* UDP datagram sockets */
    hints.ai_flags = AI_PASSIVE;     /* Let getaddrinfo() chose an address for me */
    
    if ((rv = getaddrinfo(NULL, argv[1], &hints, &serv_info)) != 0)
    {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        
        exit(1);
    }
    
    /* Loop through the list of addrinfos and bind to the first one we can */
    for(p = serv_info; p != NULL; p = p->ai_next)
    {
        if ((sock_fd = socket(p->ai_family, p->ai_socktype,
                              p->ai_protocol)) == -1)
        {
            perror("UDP server: socket");
            
            continue;
        }

        if (bind(sock_fd, p->ai_addr, p->ai_addrlen) == -1)
        {
            close(sock_fd);
            perror("UDP server: bind");
            
            continue;
        }

        break;
    }

    if (p == NULL)
    {
        fprintf(stderr, "UDP server: failed to bind socket\n");
        
        return 2;
    }

    freeaddrinfo(serv_info);

    printf("UDP Server: waiting to recvfrom...\n");
    
    addr_len = sizeof their_addr;
    while (1)
    {
        memset(buf, 0, BUF_SIZE);
        
        if ((num_bytes = recvfrom(sock_fd, buf, BUF_SIZE-1 , 0,
            (struct sockaddr *)&their_addr, &addr_len)) == -1)
        {
            perror("recvfrom");
            
            exit(1);
        }
        
        printf("UDP Server: got packet from %s\n", inet_ntop(their_addr.ss_family,
                                                get_in_addr((struct sockaddr *)&their_addr),
                                                s, sizeof s));
        
        /* Start with reply_msg being null */
        reply_msg = NULL;
        /* Send the command to be parsed and run */
        parse_and_run_command(buf, num_bytes, &reply_msg);
        
        /* Send the reply back to the sender */
        num_bytes = sendto(sock_fd, reply_msg, strlen(reply_msg), 0,
                           (struct sockaddr *) &their_addr, addr_len);
        if (num_bytes < 0) 
        {
            perror("sendto");

        }
        
        free(reply_msg);
    }

        close(sock_fd);
    
    return 0;
}