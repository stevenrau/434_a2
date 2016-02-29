/**
 * UDP proxy that forwards client TCP requests to the
 * UDP server and then forwards the server response back to
 * the client
 * 
 * CMPT 434 - A1
 * Steven Rau
 * scr108
 * 11115094
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>

#include "server.h"

/*-----------------------------------------------------------------------------
 * File-scope constants & globals
 * --------------------------------------------------------------------------*/

#define PROXY_BACKLOG   10  /* How many pending connections the socket queue will hold */
#define BUF_SIZE  1024

/*-----------------------------------------------------------------------------
 * Helper Functions
 * --------------------------------------------------------------------------*/

bool transfer_tcp_to_udp(int client_fd, int server_fd, struct addrinfo *addr)
{
    char buf[BUF_SIZE];
    int num_bytes;
    bool success = true;
    
    memset(buf, 0, BUF_SIZE);
    
    /* Read in the TCP sender's byte stream */
    num_bytes = read(client_fd, buf, BUF_SIZE);
    if (num_bytes < 0) 
    {
        fprintf(stderr, "ERROR reading from sender socket\n");
        
        success = false;
    }
    else
    {
        /* Then write it to the UDP receiver's fd */
        if ((num_bytes = sendto(server_fd, buf, BUF_SIZE, 0,
                                addr->ai_addr, addr->ai_addrlen)) == -1)
        {
            perror("talker: sendto");
        
            success = false;
        }
    }
    
    return success;
}

bool transfer_udp_to_tcp(int server_fd, int client_fd, struct addrinfo *server_addr)
{
    char buf[BUF_SIZE];
    int num_bytes;
    bool success = true;
    
    memset(buf, 0, BUF_SIZE);
    
    /* Read in the UDP server's reply */
    if ((num_bytes = recvfrom(server_fd, buf, BUF_SIZE-1 , 0,
         server_addr->ai_addr, &server_addr->ai_addrlen)) == -1)
    {
        perror("recvfrom");
            
        success = false;
    }
    else
    {
        /* Then write it to the TCP receiver's file descriptor */
        num_bytes = write(client_fd, buf, num_bytes);
        if (num_bytes == -1)
        {   
            fprintf(stderr, "ERROR writing to receiver socket\n");
            
            success = false;
        }
    }
    
    return success;
}
    

/**
 * Handles a TCP request from a client and forwards data to UDP server. Then takes a
 * UDP reply message and forwards it back to the client via TCP
 * 
 * @param[in] client  File descriptor of client stream (TCP)
 * @param[in] remote_host  Host name of server (UDP)
 * @param[in] remote_port  Server's port number (UDP)
 */
void handle(int client, char *remote_host, char *remote_port)
{
    int sock_fd;
    struct addrinfo hints;
    struct addrinfo *serv_info;
    struct addrinfo *p;
    int rv;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;

    if ((rv = getaddrinfo(remote_host, remote_port, &hints, &serv_info)) != 0)
    {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        
        exit(1);
    }

    /* Loop through the results and make the first available socket */
    for(p = serv_info; p != NULL; p = p->ai_next)
    {
        if ((sock_fd = socket(p->ai_family, p->ai_socktype,
                              p->ai_protocol)) == -1)
        {
            perror("talker: socket");
            
            continue;
        }

        break;
    }

    if (p == NULL)
    {
        fprintf(stderr, "talker: failed to create socket\n");
        
        exit(1);
    }
    
    /* Transfer the TCP client message to the UDP server */
    transfer_tcp_to_udp(client, sock_fd, p);
    
    /* Transfer the UDP server reply to the TCP client */
    transfer_udp_to_tcp(sock_fd, client, p);

    freeaddrinfo(serv_info);

    close(sock_fd);
}

/*-----------------------------------------------------------------------------
 *
 * --------------------------------------------------------------------------*/

int main(int argc, char **argv)
{
    int status;
    int sock_fd;
    int newsock;
    struct addrinfo hints;
    struct addrinfo *local_serv_info;
    struct addrinfo *cur_info;       /* Used to iterate through the server_info list */
    struct sockaddr_in6 client_addr; /* Client's address information */
    int reuse_addr = 1; /* True */
    socklen_t size; 
    char client_addr_string[BUF_SIZE];
    char *local_port;
    char *remote_host;
    char *remote_port;

    /* Get the server host and port from the command line */
    if (argc < 4)
    {
        fprintf(stderr, "Usage: %s <local_port> <remote_host> <remote_port>\n", argv[0]);
        
        exit(1);
    }
    local_port = argv[1];
    remote_host = argv[2];
    remote_port = argv[3];
    
    if (atoi(local_port) < MIN_PORT_NUM || atoi(local_port) > MAX_PORT_NUM ||
        atoi(remote_port) < MIN_PORT_NUM || atoi(remote_port) > MAX_PORT_NUM)
    {
        fprintf(stderr, "Usage: Port numbers must be between %d and %d\n", 
                         MIN_PORT_NUM, MAX_PORT_NUM);
        
        exit(1);
    }
    
    /* Get the address info */
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET6;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;     /* Let getaddrinfo() chose an address for me */
    
    if ((status = getaddrinfo(NULL, local_port, &hints, &local_serv_info)) != 0)
    {
        fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
        
        exit(1);
    }
    
    /* local_serv_info now points to a linked list of 1 or more struct addrinfos
       Loop through the list and bind to the first one we can */
    for(cur_info = local_serv_info; cur_info != NULL; cur_info = cur_info->ai_next)
    {
        if ((sock_fd = socket(cur_info->ai_family, cur_info->ai_socktype,
                              cur_info->ai_protocol)) == -1)
        {
            perror("server: socket");
            
            continue;
        }

        if (setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &reuse_addr, sizeof(int)) == -1)
        {
            perror("setsockopt");
            
            exit(1);
        }

        if (bind(sock_fd, cur_info->ai_addr, cur_info->ai_addrlen) == -1)
        {
            close(sock_fd);
            perror("server: bind");
            
            continue;
        }

        break;
    }
    
    freeaddrinfo(local_serv_info); /* Free the linked-list */
    
    /* Announce that this socket will be used to listen for incoming connections */
    if (listen(sock_fd, PROXY_BACKLOG) == -1)
    {
        perror("listen");
        
        exit(1);
    }
    
    printf("UDP proxy: waiting for connections...\n");
    
    /* Main proxy accept loop that receives requests and forwards them to the server */
    while (1)
    {
        size = sizeof(struct sockaddr_in6);
        newsock = accept(sock_fd, (struct sockaddr*)&client_addr, &size);

        if (newsock == -1)
        {
            perror("accept");
            
            continue;
        }
        else
        {
            /* Convert the IPv6 addr to human readable string */
            inet_ntop(AF_INET6, &(client_addr.sin6_addr), client_addr_string, BUF_SIZE);
            printf("\n UDP Proxy: Connection received from (%s , %d)\n",
                    client_addr_string, ntohs(client_addr.sin6_port));
            
            /* Handle the new request */
            handle(newsock, remote_host, remote_port);
        }
        
        /* Close the client socket fd now that the request is complete (non-persistent) */
        close(newsock);
    }

    close(sock_fd);
    
    return 0;
}