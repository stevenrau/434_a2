/**
 * UDP sender that takes in the receiver's IP address and Port number
 * as command line arguments.
 * 
 * Sent data is input as text on the command line
 * 
 * CMPT 434 - A2
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
#include <time.h>

#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>

#include "sender.h"
#include "shared.h"

/*-----------------------------------------------------------------------------
 * File-scope constants & globals
 * --------------------------------------------------------------------------*/

#define SENDER_BACKLOG   10  /* How many pending connections the socket queue will hold (not really necessary*/
#define BUF_SIZE  1024

/* Indicates the last sequence number that was acked */
uint32_t last_ack = UINT32_MAX;

/* An array of messages large enough to hold the max window size number of messages */
struct message *window;

/* Number of messages queued in the window (yet to receive ack for) */
uint32_t num_queued = 0;


/*-----------------------------------------------------------------------------
 * Helper Functions
 * --------------------------------------------------------------------------*/

/**
 * Takes in a msg buffer and sends it to the reciever
 * 
 * @param[in] msg      Buffer containing the message to send
 * @param[in] recv_fd  Receiver's file descriptor
 * @param[in] addr     Receiver's adress info
 * 
 */
bool transfer_msg_to_receiver(char msg[], int recv_fd, struct addrinfo *addr)
{
    int num_bytes_sent;
    bool success = true;
    
    num_bytes_sent = sendto(recv_fd, msg, sizeof(struct message), 0, addr->ai_addr, addr->ai_addrlen);
    if (num_bytes_sent == -1)
    {
        perror("recvfrom");
            
        success = false;
    }
    
    return success;;
}

/**
 * Gets an ack (reply) from the receiver
 * 
 * The ack should be the sequence number of most recent successfully recieved packet
 */
bool get_reply_from_receiver(uint32_t *reply_seq, int recv_fd, struct addrinfo *addr,
                             struct timeval *timeout)
{
    int num_bytes;
    bool success = true;
    int rv;
    
    /* Create a socket file descriptor set containing the receiver's fd for the select call */
    fd_set socket_read_set;
    FD_ZERO(&socket_read_set);
    FD_SET(recv_fd, &socket_read_set);
    
    /* Use select to see if the receiver's socket is ready for reading (has sent a reply) */
    rv = select(recv_fd + 1, &socket_read_set, NULL, NULL, timeout);
    
    if (rv > 0)
    {
        /* Read in the UDP server's reply */
        if ((num_bytes = recvfrom(recv_fd, (char *)reply_seq, sizeof(*reply_seq) , 0,
            addr->ai_addr, &addr->ai_addrlen)) == -1)
        {
            perror("recvfrom");
                
            success = false;
        }
    }
    /* If select doesn't return that a socket is ready, either timeout or error occured */
    else if (rv == 0)
    {
        printf("Timed out waiting for reply.\n");
        
        success = false;
    }
    else
    {
        perror("select");
        
        success = false;
    }
    
    return success;
}

/**
 * Updates the sliding window state according to the ack received and msg sent
 * 
 * @param[in] seq_recvd  The sequence number received as an ack
 * @param[in] msg        The message which was sent that the seq number was returned for
 */
void update_window(uint32_t *seq_recvd, struct message *msg)
{
    uint32_t index = 0;
    int i = 0;
    int num_rem = 0;
    
    /* If the sequence number received, was valid, remove all messages from the window with a
     * lower or equal sequence number since they must have already been received successfully */
    if (seq_recvd != NULL)
    {
        while (index < num_queued)
        {
            /* Stop once we get to the message with a higher sequence number than the one returned */
            if (window[index].seq > *seq_recvd)
            {
                break;
            }
            /* Else get rid of any messages in the window that have lower sequence numbers */
            else
            {
                memset(&window[index], 0, sizeof(struct message));
                
                num_rem++;
            }
            
            index++;
        }
        
        /* Copy the messages with higher sequence numbers to the front of the window */
        while (index < num_queued)
        {
            window[i] = window[index];
            
            index++;
            i++;
        }
        
        /* Update the new window size */
        num_queued -= num_rem;
        
        /* Check to see if the recent message needs to be added to the front of the window */
        if (*seq_recvd < msg->seq && (num_queued == 0 || window[num_queued-1].seq < msg->seq))
        {
            memcpy(&window[num_queued], msg, sizeof(struct message));
            
            num_queued++;
        }
        
        /* Update the last ack */
        if (*seq_recvd > last_ack || last_ack == UINT32_MAX)
        {
            last_ack = *seq_recvd;
        }
    }
    /* Else no ack was received, so place the message in the queue, if not already added */
    else
    {
        if (num_queued == 0 || window[num_queued-1].seq < msg->seq)
        {
            memcpy(&window[num_queued], msg, sizeof(struct message));
            
            num_queued++;
        }
    }
    
    printf("Window: ");
    for (i = 0; i< num_queued; i++)
    {
        printf("| %i ", window[i].seq);
    }
    printf("|\n\n");
}
    

/**
 * Forwards message data to UDP receiver/server. Then takes in
 * and handles a UDP reply (ack)
 * 
 * @param[in] msg  Message to forward to the reciever
 * @param[in] receiver_ip  Host name of server (UDP)
 * @param[in] receiver_port  Server's port number (UDP)
 * @param[in] timeout Amount of time to wait for response before timing out and moving on
 */
void handle(char msg[], char *receiver_ip, char *receiver_port, struct timeval *timeout)
{
    int receiver_sock;
    struct addrinfo hints;
    struct addrinfo *serv_info;
    struct addrinfo *p;
    int rv;
    uint32_t *reply_seq;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;

    /* Get the receiver's address information */
    if ((rv = getaddrinfo(receiver_ip, receiver_port, &hints, &serv_info)) != 0)
    {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        
        exit(1);
    }

    /* Loop through the results and make the first available socket */
    for(p = serv_info; p != NULL; p = p->ai_next)
    {
        if ((receiver_sock = socket(p->ai_family, p->ai_socktype,
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
    
    /* Transfer the message to the receiver via UDP */
    transfer_msg_to_receiver(msg, receiver_sock, p);
    
    /* Make space for the reply sequence number */
    reply_seq = calloc(1, sizeof(*reply_seq));
    
    /* Get the ack (reply) from the receiver */
    if (get_reply_from_receiver(reply_seq, receiver_sock, p, timeout))
    {
        /* If we got a valid reply, print the ack value received */
        printf("Ack received: %i\n", *reply_seq);
    }
    else
    {
        /* If the reply failed, set the reply sequence number to NULL */
        reply_seq = NULL;
    }
    
    /* Then update the sliding window */
    update_window(reply_seq, (struct message *)msg);
    
    freeaddrinfo(serv_info);

    close(receiver_sock);
    
    free(reply_seq);
}

/**
 * Process the queue of messages in teh sliding window
 * 
 * This method gets called once the window has filled
 * 
 * @param[in] receiver_ip  Host name of server (UDP)
 * @param[in] receiver_port  Server's port number (UDP)
 * @param[in] timeout Amount of time to wait for response before timing out and moving on
 */
void process_window(char *receiver_ip, char *receiver_port, struct timeval *timeout)
{
    int i;
    struct message *msg_queue_ptr = NULL;
    struct message *msg_out;
    
    /* Find the message in the queue that has a sequence # one greater than the last successfully
     * acked sequence number */
    for (i = 0; i < num_queued; i++)
    {
        if (window[i].seq == (last_ack + 1))
        {
            /* Have the message queue ptr point to the message we want to send */
            msg_queue_ptr = &window[i];
            
            break;
        }
    }
    
    /* Make space for the output message */
    msg_out = calloc(1, sizeof(struct message));
    
    /* Copy the message from the queue into the output buffer */
    memcpy(msg_out, msg_queue_ptr, sizeof(struct message));
    
    /* Send the message to the handler */
    handle((char *)msg_out, receiver_ip, receiver_port, timeout);
    
    free(msg_out);
}
    
    


/*-----------------------------------------------------------------------------
 *
 * --------------------------------------------------------------------------*/

int main(int argc, char **argv)
{
    int max_window_size;
    int timeout_sec;
    char *receiver_ip;
    char *receiver_port;
    uint32_t seq_num = 0;  /* Use 32-bit sequence num to ensure max capacity before wrapping (likely unnecesary)*/
    struct message *msg;
    char *in_buf = NULL;   /* Buffer to hold the output message sent to the receiver */
    size_t len = 0;        /* Length of text line read in */
    char *out_buf = NULL; /* Buffer to hold the output message struct containing text and seq num */
    struct timeval *timeout;

    /* Get the receiver host and port as well as window size and timeout from the command line */
    if (argc < 5)
    {
        fprintf(stderr, "Usage: %s <receiver_ip> <receiver_port> <max_window_size> <timeout_sec>\n", argv[0]);
        
        exit(1);
    }
    receiver_ip = argv[1];
    receiver_port = argv[2];
    max_window_size = atoi(argv[3]);
    timeout_sec = atoi(argv[4]);
    
    /* Check to make sure input variables are allowed */
    if (atoi(receiver_port) < MIN_PORT_NUM || atoi(receiver_port) > MAX_PORT_NUM)
    {
        fprintf(stderr, "Usage: Port numbers must be between %d and %d\n", 
                         MIN_PORT_NUM, MAX_PORT_NUM);
        
        exit(1);
    }
    
    printf("UDP sender started: \n"
           "\tReceiver IP/Hostname: %s, Receiver Port: %s\n"
           "\tMax message window size: %i  Timeout (sec): %i\n"
	   "\tReady for input...\n\n", receiver_ip, receiver_port, max_window_size, timeout_sec);
    
    /* Allocate space for the sliding window */
    window = calloc(max_window_size, sizeof(struct message));
    
    /* Make space for the timeout timeval struct */
    timeout = calloc(1, sizeof(struct timeval));
    
    /* Make space for the output buffer to hold the message */
    out_buf = calloc(1, sizeof(struct message));
    
    /* Main sender loop that receives user input from stdin and forwards the messages to the receiver
     * via UDP. The user-specified window size and timeout length determine the functional details
     * of the go-back-n sliding window implementation */
    while (1)
    {      
        in_buf = NULL;
        
        /* Setup or reset timeout since select() modifes it */
        timeout->tv_usec = 0;
        timeout->tv_sec = timeout_sec;
        
        /* If the window isn't full, try to send another new message */
        if (num_queued < max_window_size)
        {
            printf("Enter a message: \n");
            
            /* Clear out the memory of the out buffer */
            memset(out_buf, 0, sizeof(struct message));
            
            /* Allocate message space */ 
            msg = calloc(1, sizeof(struct message));
            
            /* Give the message the next sequence number and increment */
            msg->seq = seq_num;
            seq_num++;
            
            /* Read the command line text into the input buffer  */
            getline(&in_buf, &len, stdin);
            
            /* Copy as many characters from the read input buffer into the message struct that will fit */
            memcpy(msg->text, in_buf, MAX_TEXT_LENGTH);
            
            /* Put the message into the output buffer */
            memcpy(out_buf, msg, sizeof(struct message));
            
            /* Handle the message (send it and get a response) */
            handle(out_buf, receiver_ip, receiver_port, timeout);

            /* Free the message space */
            free(msg);
            
            /* Free the space allocated for the in buffer in getline() */
            free(in_buf);
        }
        /* Otherwise handle the queued messages */
        else
        {
            process_window(receiver_ip, receiver_port, timeout);
        }
 
    }
    
    return 0;
}