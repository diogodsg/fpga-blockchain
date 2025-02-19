#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <openssl/sha.h>
#include <microhttpd.h>

#define MAX_HASH_LENGTH 65
#define PORT 8080

typedef struct Block {
    int index;
    char previous_hash[MAX_HASH_LENGTH];
    char data[65];
    char hash[MAX_HASH_LENGTH];
    struct Block *next;
} Block;

Block *blockchain = NULL;
Block *last_block = NULL;

void compute_hash(Block *block, char *output) {
    char input[512];
    snprintf(input, sizeof(input), "%d%s%s", block->index, block->previous_hash, block->data);
    
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256((unsigned char *)input, strlen(input), hash);
    
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        sprintf(output + (i * 2), "%02x", hash[i]);
    }
    output[MAX_HASH_LENGTH - 1] = '\0';
}

Block *create_genesis_block() {
    Block *genesis = (Block *)malloc(sizeof(Block));
    genesis->index = 0;
    strcpy(genesis->previous_hash, "0");
    strcpy(genesis->data, "Genesis Block");
    compute_hash(genesis, genesis->hash);
    genesis->next = NULL;
    return genesis;
}

Block *add_block(const char *data) {
    if (!blockchain) {
        blockchain = create_genesis_block();
        last_block = blockchain;
    }
    
    Block *new_block = (Block *)malloc(sizeof(Block));
    new_block->index = last_block->index + 1;
    strcpy(new_block->previous_hash, last_block->hash);
    strncpy(new_block->data, data, 255);
    new_block->data[255] = '\0';
    compute_hash(new_block, new_block->hash);
    new_block->next = NULL;
    last_block->next = new_block;
    last_block = new_block;
    
    printf("Block added: Index %d, Data: %s, Hash: %s\n", new_block->index, new_block->data, new_block->hash);
    return new_block;
}

void free_blockchain() {
    Block *current = blockchain;
    while (current) {
        Block *temp = current;
        current = current->next;
        free(temp);
    }
    blockchain = last_block = NULL;
}

int send_response(struct MHD_Connection *connection, const char *message, int status_code) {
    struct MHD_Response *response = MHD_create_response_from_buffer(strlen(message), (void *)message, MHD_RESPMEM_PERSISTENT);
    int ret = MHD_queue_response(connection, status_code, response);
    MHD_destroy_response(response);
    return ret;
}
int request_handler(void *cls, struct MHD_Connection *connection, const char *url, const char *method, const char *version, const char *upload_data, size_t *upload_data_size, void **con_cls) {
    static char post_data[256] = "";

    if (strcmp(method, "GET") == 0 && strcmp(url, "/blocks") == 0) {
        char response[2048] = "";
        Block *current = blockchain;
        while (current) {
            char block_info[256];
            snprintf(block_info, sizeof(block_info), "Index: %d, Data: %s, Hash: %s\n", current->index, current->data, current->hash);
            strcat(response, block_info);
            current = current->next;
        }
        return send_response(connection, response, MHD_HTTP_OK);
    }
    else if (strcmp(method, "POST") == 0 && strcmp(url, "/block") == 0) {
        if (*con_cls == NULL) { 
            *con_cls = malloc(256); // Allocate memory for storing POST data
            return MHD_YES;
        }

        if (*upload_data_size > 0) {
            strncpy((char*)*con_cls, upload_data, *upload_data_size);
            ((char*)*con_cls)[*upload_data_size] = '\0';
            *upload_data_size = 0; 
            return MHD_YES;
        }

        if (*upload_data_size == 0) { 
            add_block((char*)*con_cls); 
            free(*con_cls);
            *con_cls = NULL; 
            return send_response(connection, "Block added", MHD_HTTP_OK);
        }
    }
    else if (strcmp(method, "DELETE") == 0 && strcmp(url, "/blocks") == 0) {
        free_blockchain();
        blockchain = create_genesis_block();
        last_block = blockchain;
        return send_response(connection, "Blockchain reset", MHD_HTTP_OK);
    }
    return send_response(connection, "Not Found", MHD_HTTP_NOT_FOUND);
}


int main() {
    blockchain = create_genesis_block();
    last_block = blockchain;

    struct MHD_Daemon *daemon = MHD_start_daemon(MHD_USE_SELECT_INTERNALLY, PORT, NULL, NULL, &request_handler, NULL, MHD_OPTION_END);
    if (!daemon) {
        fprintf(stderr, "Failed to start server\n");
        return 1;
    }
    printf("Server running on port %d...\n", PORT);
    getchar();
    MHD_stop_daemon(daemon);
    free_blockchain();
    return 0;
}
