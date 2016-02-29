/**
 * TCP server that handles simple storage-lookup requests
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
#include "storage.h"

/*-----------------------------------------------------------------------------
 * File-scope constants & globals
 * --------------------------------------------------------------------------*/

#define SERV_BACKLOG   10  /* How many pending connections the socket queue will hold */
#define BUF_SIZE  1024

/* The storage "dictionary" array held in storage.c */
extern struct key_val_pair storage[MAX_STORAGE_SIZE];

/*-----------------------------------------------------------------------------
 *
 * --------------------------------------------------------------------------*/

int main(int argc, char *argv[])
{
    int status;
    int sock_fd;
    int reuse_addr = 1;  /* Used to set the setsockopt() option SO_REUSEADDR to "yes/1" */
    int child_fd;        /* File descriptor of the accepted socket */
    int num_bytes_read;
    uint32_t port_num;
    socklen_t sin_size;
    char recv_data[BUF_SIZE];
    char client_addr_string[BUF_SIZE];
    char *reply_msg;
    struct addrinfo hints;           /* Instructs getaddrinfo() what to get for us */
    struct addrinfo *server_info;    /* The results of the getaddrinfo() call */
    struct addrinfo *cur_info;       /* Used to iterate through the server_info list */
    struct sockaddr_in6 client_addr; /* Client's address information */
    
    memset(storage, 0, sizeof(struct key_val_pair) * MAX_STORAGE_SIZE); /* Zero out the storage */
    
    memset(&hints, 0, sizeof(hints)); /* Ensure that the hints struct is zeroed out */
    hints.ai_family = AF_INET6;       /* Use IPv6 */
    hints.ai_socktype = SOCK_STREAM;  /* TCP stream sockets */
    hints.ai_flags = AI_PASSIVE;      /* Let getaddrinfo() chose an address for me */
    
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
        

    if ((status = getaddrinfo(NULL, argv[1], &hints, &server_info)) != 0)
    {
        fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
        
        exit(1);
    }

    /* server_info now points to a linked list of 1 or more struct addrinfos
       Loop through the list and bind to the first one we can */
    for(cur_info = server_info; cur_info != NULL; cur_info = cur_info->ai_next)
    {
        if ((sock_fd = socket(cur_info->ai_family, cur_info->ai_socktype,
                              cur_info->ai_protocol)) == -1)
        {
            perror("TCP server: socket");
            
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
            perror("TCP server: bind");
            
            continue;
        }

        break;
    }

    freeaddrinfo(server_info); /* Free the linked-list */
    
    /* If we made it to the end of the list without succeeding to bind, exit */
    if (cur_info == NULL) 
    {
        fprintf(stderr, "TCP server: failed to bind\n");
        
        exit(1);
    }
    
    /* Announce that this socket will be used to listen for incoming connections */
    if (listen(sock_fd, SERV_BACKLOG) == -1)
    {
        perror("listen");
        
        exit(1);
    }
    
    printf("TCP server: waiting for connections...\n");
    
    /* Main accept loop */
    while(1)
    {
        sin_size = sizeof(struct sockaddr_in6);
        
        child_fd = accept(sock_fd, (struct sockaddr *)&client_addr, &sin_size);
        if (child_fd == -1)
        {
            perror("accept");
            
            continue;
        }

        /* Convert the IPv6 addr to human readable string */
        inet_ntop(AF_INET6, &(client_addr.sin6_addr), client_addr_string, BUF_SIZE);
        printf("\n Server: Connection received from (%s , %d)\n",
                client_addr_string, ntohs(client_addr.sin6_port));
        
        /* Zero out the receive buffer */
        memset(recv_data, 0, BUF_SIZE);
        
        /* Read in the client's request string */
        num_bytes_read = read(child_fd, recv_data, BUF_SIZE);
        if (num_bytes_read < 0) 
        {
            fprintf(stderr, "ERROR reading from socket\n");
        }
        
        /* Start with reply_msg being null */
        reply_msg = NULL;
        
        /* Send the command to be parsed and run */
        parse_and_run_command(recv_data, num_bytes_read, &reply_msg);
        
        /* Send back a reply indicating the results of the request */
        num_bytes_read = write(child_fd, reply_msg, strlen(reply_msg));
        if (num_bytes_read < 0)
        {
            fprintf(stderr, "ERROR writing to socket\n");
        }
        
        free(reply_msg);
        
        close(child_fd);
    }
    
    return 0;
}