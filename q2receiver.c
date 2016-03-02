/**
 * UDP receiver (server) that takes in messages and
 * replies with an ack indicating the sequence number
 * of the most recently received message.
 * 
 * Contains a control interface to manually choose to
 * "corrupt" a received message.
 * 
 * CMPT 434 - A2
 * Steven Rau
 * scr108
 * 11115094
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include "shared.h"


/*-----------------------------------------------------------------------------
 * File-scope constants & globals
 * --------------------------------------------------------------------------*/

/* Keeps track of the last successful sequence number received */
uint32_t last_succ_seq = UINT32_MAX;

/* An array of messages large enough to hold the max allowable amount of buffered messages */
struct message *buffer;

/* Number of messages buffered in the window (yet to receive ack for) */
uint32_t num_buffed= 0;

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

/**
 * Gets a bool determining if an ack should be viewed as corrupt or lost (ie. don't
 * send the ack)
 * 
 * @param[in] prob  Probability that the ack should be considered corrupt (between 0 and 1)
 * 
 * Returns true or false based on a given probability
 */
bool ackLost(float prob)
{
    /* Generate s random float between 0 and 1 */
    float rand_num = (float)rand() / (float)RAND_MAX; 
    
    return rand_num < prob;
}

/**
 * Adds an out of order message to the buffer.
 * 
 * @param[in] msg  The message to add to the buffer
 */
void buffer_msg(struct message *msg)
{
    int i;
    
    /* Don't do anything if we've already successfully received this message */
    if (msg->seq <= last_succ_seq && last_succ_seq != UINT32_MAX)
    {
        return;
    }
    
    /* Check to see if this message can be added to the buffer */
    if (num_buffed == 0 || buffer[num_buffed-1].seq < msg->seq)
    {
        memcpy(&buffer[num_buffed], msg, sizeof(struct message));
        
        num_buffed++;
    }
    
    printf("\tBuffer: ");
    for (i = 0; i< num_buffed; i++)
    {
        printf("| %i ", buffer[i].seq);
    }
    printf("|\n\n");
}

/**
 * Checks the buffer to see if anything can be cleared given the new in-order sequence number
 * 
 * @param[out] seq  Indicates the new in-order sequence number received. Updated to be the largest
 *                  sequence number of messages that were able to be cleared from the buffer
 */
void clear_buffer_check(uint32_t *new_seq)
{
    int i = 0;
    int index = 0;
    int num_rem = 0;
    
    if (num_buffed == 0)
    {
        return;
    }
    
    while (i < num_buffed)
    {
        /* Go through the buffer and increment the sequence number depending on how many
         * messages can be cleared */
        if (buffer[i].seq == (*new_seq + 1))
        {
            *new_seq = *new_seq + 1;
            
            printf("\tCleared from buffer:  Seq #: %i   Text: %s\n", buffer[i].seq,  buffer[i].text);
            
            /* Clear the message from the buffer */
            memset(&buffer[i], 0, sizeof(struct message));
            
            /* Keep track of the number of messages removed */
            num_rem++;
        }
        else
        {
            /* If the buffered messages are not consecutive, stop */
            break;
        }
        
        i++;
    }
    
    /* If messages were cleared, shift remaining items to the front of the buffer */
    while (i < num_buffed && i != index)
    {
        buffer[index] = buffer[i];
        
        index++;
        i++;
    }
    
    /* Update the new number of buffered messages */
    num_buffed -= num_rem;
    
    printf("\tNew most recent sequence number after buffer clear: %i\n", *new_seq);
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
    uint32_t reply_seq;  /* Sequence number received */
    socklen_t addr_len;
    char s[INET6_ADDRSTRLEN];
    struct message *msg;
    char *msgRecvd = NULL;  /* Buffer to read in the yes/no message corrupt input */
    size_t len = 0;         /* Length of text line read in */
    float ack_loss_prob;
    int buff_size;
    
    if (argc < 4)
    {
        fprintf(stderr, "Usage: %s <port_number> <ack_loss_prob> <buffer_size>\n", argv[0]);
        
        exit(1);
    }
    
    /* Grab the ack loss probability from the command line */
    ack_loss_prob = atof(argv[2]);
    
    /* Grab the buffer size from the commmand line and allocate buffer space */
    buff_size = atoi(argv[3]);
    buffer = calloc(buff_size, sizeof(struct message));
    
    
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
    
    addr_len = sizeof their_addr;

    freeaddrinfo(serv_info);
    
    /* Allocate space for the message to be received */
    msg = calloc(1, sizeof(struct message));

    printf("UDP Server: waiting to recvfrom...\n");

    /* Main receiver loop that takes in messages from the sender and handles them according to
     * their sequence number. Out of order messages can be buffered until the next in-order 
     * message is received */
    while (1)
    {
        /* Clear out the message space */
        memset(msg, 0, sizeof(struct message));
        memset(msgRecvd, 0, len);
        
        /* Receive the message */
        if ((num_bytes = recvfrom(sock_fd, msg, sizeof(struct message) , 0,
            (struct sockaddr *)&their_addr, &addr_len)) == -1)
        {
            perror("recvfrom");
            
            exit(1);
        }
        
        printf("\nUDP Server: got packet from %s", inet_ntop(their_addr.ss_family,
                                                get_in_addr((struct sockaddr *)&their_addr),
                                                s, sizeof s));
        
        /* Print the message info that was received */
        printf("\nMsg recvd:  Seq #: %i   Text: %s", msg->seq, msg->text);
        
        /* If the message is the next in-order message, reply with the sequence number received */
        if (last_succ_seq == (msg->seq - 1))
        {
            reply_seq = msg->seq;
            
            /* Get user inpt to decide whether the data received was "corrupt" (i.e., no ack) */
            printf("\tThis is the next in-order message\n"
                   "\tShould the message be correctly received? (y/n) \n\t");
            getline(&msgRecvd, &len, stdin);
            
            /* If the first letter Y or y (yes), send a reply */
            if (msgRecvd[0] == 'y' || msgRecvd[0] == 'Y')
            {   
                /* Do any potential clearing of the buffer now that we have an in-order emssage */
                clear_buffer_check(&reply_seq);
                
                /* If the ack shouldn't be considered lost/corrupt, send a reply */
                if(!ackLost(ack_loss_prob))
                {
                    /* Send the sequence number successfully received as a reply back to the sender */
                    num_bytes = sendto(sock_fd, (char *)&reply_seq, sizeof(reply_seq), 0,
                                    (struct sockaddr *) &their_addr, addr_len);
                    if (num_bytes < 0) 
                    {
                        perror("sendto");

                    }
                    
                    printf("\tAck sent\n");
                }
                else
                {
                    printf("\tAck was corrupted\n");
                }
                
                /* Update the last successful sequence number received */
                last_succ_seq = reply_seq;
            }
        }
        /* Else if the message received has the same sequence number as the most recently successful one,
         * send the acknowledgement again */
        else if (last_succ_seq == msg->seq)
        {
            reply_seq = msg->seq;
            
            printf("\tThis is a retransmission of the last correctly received in-order message\n");
            
            /* If the ack shouldn't be considered lost/corrupt, send a reply */
            if(!ackLost(ack_loss_prob))
            {
                /* Send the sequence number successfully received as a reply back to the sender */
                num_bytes = sendto(sock_fd, (char *)&reply_seq, sizeof(reply_seq), 0,
                                (struct sockaddr *) &their_addr, addr_len);
                if (num_bytes < 0) 
                {
                    perror("sendto");

                }
                
                printf("\tAck sent\n");
            }
            else
            {
                printf("\tAck was corrupted\n");
            }
        }
        /* Else the received message is neither the next in-order message nor a retransmission of the most
         * recent in-order message, buffer it if received correctly, and loop back to receive again */
        else
        {
            printf("\tThis is an out of order message.\n"
                   "\tShould the message be correctly received? (y/n) \n\t");
            getline(&msgRecvd, &len, stdin);
            
            /* If the first letter Y or y (yes), buffer the message */
            if (msgRecvd[0] == 'y' || msgRecvd[0] == 'Y')
            {
                if (num_buffed < buff_size)
                {
                    buffer_msg(msg);
                    
                    printf("\tMessage buffered\n");
                }
                else
                {
                    /* Should never happen if size is chosen wisely */
                    printf("\tNo space left in buffer. Message discarded\n");
                }
                
                /* If  we have received at least one successful message send an ack
                 * of he most recently successfuly sequence number to prevent an
                 * endless loop on the client end */
                if (last_succ_seq != UINT32_MAX)
                {
                    /* If the ack shouldn't be considered lost/corrupt, send a reply */
                    if(!ackLost(ack_loss_prob))
                    {
                        reply_seq = last_succ_seq;
                        
                        /* Send the sequence number last successfully received as a 
                         * reply back to the sender */
                        num_bytes = sendto(sock_fd, (char *)&reply_seq, sizeof(reply_seq), 0,
                                        (struct sockaddr *) &their_addr, addr_len);
                        if (num_bytes < 0) 
                        {
                            perror("sendto");

                        }
                        
                        printf("\tAck sent\n");
                    }
                    else
                    {
                        printf("\tAck not sent (either corrupt\n");
                    }
                }
            }
        }
    }
    
    /* Free the buffer holding the user's input */
    free(msgRecvd);
    
    free(msg);

    close(sock_fd);
    
    return 0;
}