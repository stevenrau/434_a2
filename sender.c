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
bool get_reply_from_receiver(uint32_t *reply_seq, int recv_fd, struct addrinfo *addr)
{
    int num_bytes;
    bool success = true;
    
    /* Read in the UDP server's reply */
    if ((num_bytes = recvfrom(recv_fd, (char *)reply_seq, sizeof(*reply_seq) , 0,
         addr->ai_addr, &addr->ai_addrlen)) == -1)
    {
        perror("recvfrom");
            
        success = false;
    }
    
    return success;
}
    

/**
 * Forwards message data to UDP receiver/server. Then takes in
 * and handles a UDP reply (ack)
 * 
 * @param[in] msg  Message to forward to the reciever
 * @param[in] receiver_ip  Host name of server (UDP)
 * @param[in] receiver_port  Server's port number (UDP)
 */
void handle(char msg[], char *receiver_ip, char *receiver_port)
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
    get_reply_from_receiver(reply_seq, receiver_sock, p);
    
    printf("Ack received: %i\n\n", *reply_seq);
    
    freeaddrinfo(serv_info);

    close(receiver_sock);
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

    /* Get the receiver host and port as well as window size and timeout from the command line */
    if (argc < 5)
    {
        fprintf(stderr, "Usage: %s <receiver_ip> <receiver_port> <max_send_window_size> <timeout_sec>\n", argv[0]);
        
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
    
    /* Make space for the output buffer to hold the message */
    out_buf = calloc(1, sizeof(struct message));
    
    /* Main sender loop that receives user input from stdin and forwards the messages to the receiver
     * via UDP. The user-specified window size and timeout length determine the functional details
     * of the go-back-n sliding window implementation */
    while (1)
    {      
        in_buf = NULL;
        
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
        handle(out_buf, receiver_ip, receiver_port);

        /* Free the message space */
        free(msg);
        
        /* Free the space allocated for the in buffer in getline() */
        free(in_buf);
    }
    
    return 0;
}