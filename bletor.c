#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <fcntl.h>      // For open()
#include <unistd.h>     // For read(), close()
#include <sys/stat.h>   
#include "benc.h"
#include "sha1.h"

int decode_command(int argc, char *argv[]) {
    
    // Check if we have enough arguments
    if (argc < 3) {
        fprintf(stderr, "Usage: %s decode <bencoded_string>\n", argv[0]);
        return -1;
    }

    char *bencoded_str= argv[2];
    int error = 0;

    struct benc *decoded = benc_decode((const uint8_t*)bencoded_str,strlen(bencoded_str),&error);

    if (!decoded) {
        fprintf(stderr, "Decoding failed: %s\n", benc_strerror(error));
        return error;
    }

    benc_print(decoded);

    printf("\n");
   
    size_t len = 0;
    uint8_t *bencoded = benc_encode(&len, (const struct benc *)decoded);
    if (!bencoded) {
        fprintf(stderr, "Error bencoding\n");
        benc_free(decoded);
    }

    fprintf(stdout, "bencoded: %.*s\n", (int)len,bencoded);

    free(bencoded);
    
    benc_free(decoded);


    return 0;
}

int info_command(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s info <torrent_file_path>\n", argv[0]);
        return -1;
    }
    
    char *torrent_file_path = argv[2];
    uint8_t *buffer = NULL;
    struct benc *parsed_torrent = NULL;
    
    // Open the file
    int fd = open(torrent_file_path, O_RDONLY);
    if (fd == -1) {
        fprintf(stderr, "Error opening file %s\n", torrent_file_path);
        goto cleanup;
    }
    
    // Get file size
    struct stat st;
    if (stat(torrent_file_path, &st) != 0) {
        fprintf(stderr, "Error getting file info for %s\n", torrent_file_path);
        goto cleanup;
    }
    
    // Allocate memory
    buffer = (uint8_t *) malloc(st.st_size);
    if (!buffer) {
        fprintf(stderr, "Memory allocation failed.\n");
        goto cleanup;
    }
    
    // Read entire file
    if (read(fd, buffer, st.st_size) != st.st_size) {
        fprintf(stderr, "Error reading file %s\n", torrent_file_path);
        goto cleanup;
    }
    
    // Parse torrent file content
    int error = 0;
    parsed_torrent = benc_decode(buffer, st.st_size, &error);
    if (!parsed_torrent || parsed_torrent->type != BENC_DICT) {
        fprintf(stderr, "Decoding torrent failed: %s\n", 
                parsed_torrent ? "Not a dictionary" : benc_strerror(error));
        goto cleanup;
    }
    
    struct benc_dict *torrent_dict = (struct benc_dict *)parsed_torrent;
    
    // Get announce URL
    struct benc_str *announce_str = benc_dict_get_str(torrent_dict, "announce");
    if (!announce_str) {
        fprintf(stderr, "Missing or invalid announce key\n");
        goto cleanup;
    }
    
    // Get info dictionary
    struct benc *info = benc_dict_get(torrent_dict, "info");
    if (!info || info->type != BENC_DICT) {
        fprintf(stderr, "Missing or invalid info key\n");
        goto cleanup;
    }
    const struct benc_dict *info_dict = (const struct benc_dict *)info;
    
    // Get file length
    struct benc *length = benc_dict_get(info_dict, "length");
    if (!length || length->type != BENC_INT) {
        fprintf(stderr, "Missing or invalid length key\n");
        goto cleanup;
    }
    struct benc_int *length_int = (struct benc_int *)length;
    
    // Print torrent information
    fprintf(stdout, "Tracker URL: %.*s\n", (int)announce_str->len, announce_str->data);
    fprintf(stdout, "Length: %lld\n", length_int->value);

    // Print info dict hash 
    size_t info_dict_len = 0;
    uint8_t *info_bencoded = benc_encode(&info_dict_len, (const struct benc *)info_dict);
    if (!info_bencoded) {
        fprintf(stderr, "Error bencoding info dict\n");
        goto cleanup;
    }

    uint8_t hash[SHA1_DIGEST_LENGTH];
    sha1_hash(info_bencoded,info_dict_len,hash);

    fprintf(stdout, "Info Hash: ");
    print_sha1(hash);

    // Print single pieces hashes 
    printf("Piece Hashes:\n");
    struct benc_str *pieces = benc_dict_get_str(info_dict, "pieces");
    if (!pieces) {
        fprintf(stderr, "Missing or invalid pieces key\n");
        goto cleanup;
    }

    size_t num_pieces = pieces->len / 20; //20 bytes for each piece
    for (size_t i = 0; i < num_pieces; i++) {
        size_t offset = i * 20;

        for (size_t j = 0; j < 20; j++) {
            printf("%02x", (unsigned char)pieces->data[offset+j]);
        }

        printf("\n");
    }

    return 0;
    
cleanup:
    if (buffer) free(buffer);
    if (parsed_torrent) benc_free(parsed_torrent);
    if (fd != -1) close(fd);
    
    return -1;
}

int main (int argc, char *argv[]) {

    if (argc < 3) {
        fprintf(stderr, "Usage: %s <command> [options]\n", argv[0]);
        return 1;
    }

    const char* command = argv[1];

    if (strcmp(command,"decode") == 0){
        return decode_command(argc,argv);
    } else if (strcmp(command, "info") == 0) {
        return info_command(argc, argv);
    } else {
        fprintf(stderr, "Unkown command %s\n", command);
        return 1;
    }
    return 0;
}
