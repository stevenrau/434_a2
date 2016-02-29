/**
 * TCP proxy that forwards client requests to the TCP server
 * and then forwards the server response back to the client
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

/**
 * Transfers bytes read from file decsriptor "in_fd" to file descriptor "out_fd"
 * 
 * @param[in] in_fd   File desciptor of input byte stream
 * @param[in] out_fd  File descriptor of the output byte stream
 * 
 * @return true if transfer was successful
 *         false otherwise
 */
bool transfer(int in_fd, int out_fd)
{
    char buf[BUF_SIZE];
    int num_bytes_read;
    int num_bytes_written;
    bool success = true;
    
    /* Read in the sender's byte stream */
    num_bytes_read = read(in_fd, buf, BUF_SIZE);
    if (num_bytes_read < 0) 
    {
        fprintf(stderr, "ERROR reading from sender socket\n");
        
        success = false;
    }
    else
    {
        /* Then write it to the receiver's file descriptor */
        num_bytes_written = write(out_fd, buf, num_bytes_read);
        if (num_bytes_written == -1)
        {   
            fprintf(stderr, "ERROR writing to receiver socket\n");
            
            success = false;
        }
    }
    
    return success;
}

/**
 * Handles a request from a client and forwards data to server. Then forwards reply
 * from the server back to the client
 * 
 * @param[in] client  File descriptor of client stream
 * @param[in] remote_host  Host name of server
 * @param[in] remote_port  Server's port numbers
 */
void handle(int client, char *remote_host, char *remote_port)
{
    struct addrinfo hints;
    struct addrinfo *serv_info;
    struct addrinfo *p;
    int server_sock = -1;
    int rv;
    char s[INET6_ADDRSTRLEN];

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    
    if ((rv = getaddrinfo(remote_host, remote_port, &hints, &serv_info)) != 0)
    {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        
        return;
    }
    
    /* serv_info now points to a linked list of 1 or more struct addrinfos
       Loop through the list and bind to the first one we can */
    for(p = serv_info; p != NULL; p = p->ai_next)
    {
        if ((server_sock = socket(p->ai_family, p->ai_socktype,
                                  p->ai_protocol)) == -1)
        {
            perror("Client: socket");
            
            continue;
        }

        if (connect(server_sock, p->ai_addr, p->ai_addrlen) == -1)
        {
            close(server_sock);
            perror("Client: connect");
            
            continue;
        }

        break;
    }

    if (p == NULL)
    {
        fprintf(stderr, "Client: failed to connect\n");
        
        return;
    }
    
    inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr),
              s, sizeof s);
    printf("Client: connecting to %s\n", s);
    
    freeaddrinfo(serv_info); /* No longer need this list */
    
    /* Send the request from the client to the server */
    transfer(client, server_sock);
    
    /* Send the reply from the server to the client */
    transfer(server_sock, client);
    
    /* We are now done the request, so close the server socket */
    close(server_sock);
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
    
    printf("TCP proxy: waiting for connections...\n");
    
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
            printf("\n Proxy: Connection received from (%s , %d)\n",
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
