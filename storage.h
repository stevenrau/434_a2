#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

/*-----------------------------------------------------------------------------
 * Constants and type defs
 * --------------------------------------------------------------------------*/

#define MAX_KEY_SIZE      10
#define MAX_VAL_SIZE      200
#define MAX_STORAGE_SIZE  256  /* Store a max of 256 key value pairs */

struct key_val_pair
{
    char key[MAX_KEY_SIZE];
    char val[MAX_VAL_SIZE];
};

/*-----------------------------------------------------------------------------
 * Function defs
 * --------------------------------------------------------------------------*/

/**
 * Returns a string representation of a key_val_pair struct
 * 
 * @NOTE The string returned by this function needs to be freed by the caller
 * 
 * @param[in] in_pair  The key_val_pair to get a string representation of
 * 
 * @return A string represetation of the provided key_val_pair
 */
char *key_value_pair_string(struct key_val_pair in_pair);

/**
 * Prints all key/value pairs currently in the "dictionary" to the console
 */
void print_all_key_value_pairs();

/**
 * Adds a key/value pair to the "dictionary"
 * 
 * @param[in] new_key  Key string to add
 * @param[in] new_val  Value string to add
 * 
 * @return true if the add was successful
 *         false if the storage is at capacity or if the key already exists
 */
bool add_pair(char *new_key,  char *new_val);

/**
 * Gets the value for a given key, if it exists in storage
 * 
 * @NOTE value string needs to be freed by the caller
 * 
 * @param[in]  in_key    The key we are looking for
 * @param[out] out_value  The value of the given key. Null if none is found
 * 
 * @return true if the key was found in storage
 *         false otherwise
 */
bool get_value_from_key(char *in_key, char **out_value);

/**
 * Gets a string representation of the entire dictionary
 * 
 * @NOTE: The string returned by this function needs to be freed by the caller
 * 
 * @return A string representation of all key/value pairs in storage
 */
char *get_all_key_val_pair_string();

/**
 * Removes the key/value pair from storage if it exists
 * 
 * @param[in] in_key  The key for the key/val pair we want to remove
 * 
 * @return true if the removal was successful
 *         false otherwise (if the key wasn't found)
 */
bool remove_key_val_pair(char *in_key);

/**
 * Takes in a command string, parses it and attempts to run it
 * 
 * @NOTE The result_string needs to be freed by the caller
 * 
 * @param[in] in_cmd  The raw command request string
 * @param[in] cmd_size Size of the command string (in bytes)
 * @param[out] result_string  String indicating the result of the operation
 * 
 * return true if the attempt to parse and run the command succeeded
 *        false otherwise
 */
bool parse_and_run_command(char *in_cmd, int cmd_size, char **result_string);

