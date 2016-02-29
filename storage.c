/**
 * Key/value pair storage and retrieval methods
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

#include "storage.h"

/*-----------------------------------------------------------------------------
 * Constants and globals
 * --------------------------------------------------------------------------*/

#define MAX_STR_SIZE  512

uint32_t storage_cnt = 0;
struct key_val_pair storage[MAX_STORAGE_SIZE];

/*-----------------------------------------------------------------------------
 * Functions
 * --------------------------------------------------------------------------*/

char *key_value_pair_string(struct key_val_pair in_pair)
{
    char *out_string = calloc(256, 1);
    
    sprintf(out_string, "Key: %s, Value: %s", in_pair.key, in_pair.val);
    
    return out_string;
}

void print_all_key_value_pairs()
{
    uint32_t i;
    for (i = 0; i < storage_cnt; i++)
    {
        char *cur_string = key_value_pair_string(storage[i]);
        printf("%s\n", cur_string);
        
        /* The string gets allocated in key_value_pair_string(), so it needs to be freed */
        free(cur_string);
    }
}

bool add_pair(char *new_key,  char *new_val)
{
    uint32_t i;
    struct key_val_pair new_pair;
    
    /* Ensure there is still space left */
    if (storage_cnt >= MAX_STORAGE_SIZE)
    {
        return false;
    }
    
    /* Do a linear search for duplicate key */
    for (i = 0; i < storage_cnt; i++)
    {
        if (strcmp(storage[i].key, new_key) == 0)
        {
            return false;
        }
    }
    
    /* Create the new key/value struct and copy the strings from the provided ptrs */
    strcpy(new_pair.key, new_key);
    strcpy(new_pair.val, new_val);
    
    storage[storage_cnt] = new_pair;
    storage_cnt++;
    
    return true;
}

bool get_value_from_key(char *in_key, char **out_value)
{
    uint32_t i;
    
    *out_value = NULL;
    
    /* Do a linear search for the key */
    for (i = 0; i < storage_cnt; i++)
    {
        if (strcmp(storage[i].key, in_key) == 0)
        {
            *out_value = calloc(sizeof(storage[i].val), 1);
            strcpy(*out_value, storage[i].val);
            
            return true;
        }
    }
    
    return false;
}

char *get_all_key_val_pair_string()
{
    uint32_t i;
    char cur_string[MAX_STR_SIZE];
    
    /* Allocate enough space for the string reps of all entries in storage */
    char *out_string = calloc(storage_cnt * (sizeof(struct key_val_pair) + strlen("Key: , Val: \n")), 1);

    for (i = 0; i < storage_cnt; i++)
    {
        memset(cur_string, 0, MAX_STR_SIZE);
        sprintf(cur_string, "Key: %s, Value: %s\n", storage[i].key, storage[i].val);
        
        strcat(out_string, cur_string);
    }
    
    return out_string;
}

bool remove_key_val_pair(char *in_key)
{
    uint32_t i;
    uint32_t j;
    
    /* Do a linear search for the key */
    for (i = 0; i < storage_cnt; i++)
    {
        if (strcmp(storage[i].key, in_key) == 0)
        {
            /* We found the key, so shift everything after the key once to the left to
               owerwrite the key_val_pair we want to remove */
            for (j = i + 1; j < storage_cnt; j++)
            {
                storage[j-1] = storage[j];
            }
            
            storage_cnt--;
            
            return true;
        }
    }
    
    return false;
}

bool parse_and_run_command(char *in_cmd, int cmd_size, char **result_string)
{
    char *command;
    char *raw_command;
    char *key = NULL;
    char *raw_key = NULL;
    char *val = NULL;
    char *raw_val = NULL;
    char *string_rep;
    bool success = false;

    /* The request should be space delimited (or end with newline) if it has arguments, 
       so use strtok() to grab the first token, which should be an operation
       command (if it's a valid request) */
    command = strtok (in_cmd," \n");
        
    /* Or the command might not have a space (single word, like getall), so get
       the entire command string minus the final terminator character
       NOTE: This might not be needed anymore now that the above strtok also uses \n
             as a delimiter. However, it still works with this redundant check, so
             I'll leave it in */
    raw_command = calloc(cmd_size-1, 1);
    strncpy(raw_command, in_cmd, cmd_size-1);

    /* The longest possible reply string is the getall msg, so make sure we can hold that */
    *result_string = calloc(MAX_STORAGE_SIZE * (sizeof(struct key_val_pair) + strlen("Key: , Val: \n")), 1);
    
    
    /* Then check if command or raw_command match any of the available operations */
    if (strcmp(command, "add") == 0 || strcmp(raw_command, "add") == 0)
    {
        printf("add command request received\n");
            
        key = strtok (NULL," ");  /* The next space delimited token should be the key */
        if (key == NULL)
        {
            sprintf(*result_string, "Invalid parameters for add operation");
        }
        else
        {
            raw_val = strtok (NULL," ");  /* Followed by the value */
            if (raw_val == NULL)
            {
                sprintf(*result_string, "Invalid parameters for add operation");
            }
            else
            {
                /* Val will have a newline as its final char, so remove it before searching */
                if (raw_val[strlen(raw_val)-1] == '\n')
                {
                    val = calloc(strlen(raw_val)-1, 1);
                    strncpy(val, raw_val, strlen(raw_val)-1);
                }
                /* Now we have the key/value, so attempt to add them to the storage array */
                if (add_pair(key, val))
                {
                    sprintf(*result_string, "Pair added (key: %s, value: %s)", key, val);
                }
                else
                {
                    sprintf(*result_string, "Failed to add key/value pair");
                }
                    
                free(val);
            }
        }
    }
    else if (strcmp(command, "getvalue") == 0 || strcmp(raw_command, "getvalue") == 0)
    {
        printf("getvalue command request received\n");
            
        raw_key = strtok (NULL," ");  /* The next space delimited token should be the key */
        if (raw_key == NULL)
        {
            sprintf(*result_string, "Invalid parameters for getvalue operation");
        }
        else
        {
            /* Key will have a newline as its final char, so remove it before searching */
            if (raw_key[strlen(raw_key)-1] == '\n')
            {
                key = calloc(strlen(raw_key)-1, 1);
                strncpy(key, raw_key, strlen(raw_key)-1);
            }
            /* Now we have the key, so attempt to get the matching value if it exists */
            if (get_value_from_key(key, &val))
            {
                sprintf(*result_string, "Key Found: Key %s has value %s", key, val);
                free(val);
            }
            else
            {
                sprintf(*result_string, "Failed to find pair for the key %s", key);
            }
                
            free(key);
        }
    }
    else if (strcmp(command, "getall") == 0 || strcmp(raw_command, "getall") == 0)
    {
        printf("getall command request received\n");
            
        string_rep = get_all_key_val_pair_string();
            
        sprintf(*result_string, "%s", string_rep);
            
        free(string_rep);
    }
    else if (strcmp(command, "remove") == 0 || strcmp(raw_command, "remove") == 0)
    {
        printf("remove command request received\n");
            
        raw_key = strtok (NULL," ");  /* The next space delimited token should be the key */
        if (raw_key == NULL)
        {
            sprintf(*result_string, "Invalid parameters for remove operation");
        }
        else
        {
            /* Key will have a newline as its final char, so remove it before searching */
            if (raw_key[strlen(raw_key)-1] == '\n')
            {
                key = calloc(strlen(raw_key)-1, 1);
                strncpy(key, raw_key, strlen(raw_key)-1);
            }
            /* Now we have the key, so attempt to remove the key/val pair if it exists */
            if (remove_key_val_pair(key))
            {
                sprintf(*result_string, "Pair with key %s successfully removed", key);
            }
            else
            {
                sprintf(*result_string, "Failed to find pair for the key %s", key);
            }
                
            free(key);
        }
    }
    else
    {
        printf("Invalid command request received\n");
            
        sprintf(*result_string, "Not a valid request");
    }
    
    free(raw_command);
    
    return success;
}
