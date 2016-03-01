/**
 * Variables shared between sender and receiver
 * 
 * CMPT 434 - A2
 * Steven Rau
 * scr108
 * 11115094
 */

#include <stdint.h>


/* Max and min allowed port numbers */
#define MIN_PORT_NUM    30000
#define MAX_PORT_NUM    40000

/* Define the max line of text length */
#define MAX_TEXT_LENGTH  256

/*
 * Message struct containing the line of text as well as a
 * sequence number.
 * 
 * The sender constructs this with a proper sequence number and
 * the lin of text from the command line entered from the user
 * 
 * The receiver will expect the input buffer to contain data in this
 * form. so it can and should be cast to this struct
 */
struct message
{
    uint32_t seq;
    char text[MAX_TEXT_LENGTH];
};
    