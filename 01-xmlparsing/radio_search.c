#include <exec/types.h>
#include <exec/memory.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#define ALIGN __attribute__((aligned(2)))

struct RadioEntry {
    char server_name[64];
    char server_type[16];
    UWORD bitrate;
    UWORD pad1;
    ULONG samplerate;
    UBYTE channels;
    UBYTE pad2[3];
    char listen_url[128];
    char current_song[128];
    char genre[32];
} ALIGN;

struct IndexEntry {
    ULONG offset;
    char server_name[64];
} ALIGN;

void load_index(const unsigned char *filename, struct IndexEntry **index, ULONG *num_entries) {
    BPTR file = Open((CONST_STRPTR)filename, MODE_OLDFILE);
    if (!file) {
        printf("Failed to open index file: %s\n", filename);
        return;
    }
    
    LONG fileSize = Seek(file, 0, OFFSET_END);
    if (fileSize <= 0) {
        printf("Empty or invalid index file (size: %ld)\n", fileSize);
        Close(file);
        return;
    }
    
    printf("Index file size: %ld bytes\n", fileSize);
    
    if (Seek(file, 0, OFFSET_BEGINNING) == -1) {
        printf("Failed to seek to beginning of index file\n");
        Close(file);
        return;
    }
    
    *num_entries = fileSize / sizeof(struct IndexEntry);
    printf("Calculated number of entries: %u\n", *num_entries);
    
    *index = AllocMem(fileSize, MEMF_CLEAR);
    if (!*index) {
        printf("Failed to allocate %ld bytes for index\n", fileSize);
        Close(file);
        return;
    }
    
    LONG bytesRead = Read(file, *index, fileSize);
    if (bytesRead != fileSize) {
        printf("Failed to read index file completely (read %ld of %ld bytes)\n", 
               bytesRead, fileSize);
        FreeMem(*index, fileSize);
        *index = NULL;
    } else {
        printf("Successfully read %ld bytes from index\n", bytesRead);
    }
    
    Close(file);
}

struct RadioEntry *load_entry(const unsigned char *filename, ULONG offset) {
    static struct RadioEntry entry;
    BPTR file = Open((CONST_STRPTR)filename, MODE_OLDFILE);
    if (!file) {
        printf("Failed to open data file: %s\n", filename);
        return NULL;
    }
    
    if (Seek(file, offset, OFFSET_BEGINNING) == -1) {
        printf("Failed to seek to offset %u\n", offset);
        Close(file);
        return NULL;
    }
    
    LONG bytesRead = Read(file, &entry, sizeof(struct RadioEntry));
    if (bytesRead != sizeof(struct RadioEntry)) {
        printf("Failed to read entry at offset %u (read %ld bytes)\n", 
               offset, bytesRead);
        Close(file);
        return NULL;
    }
    
    Close(file);
    return &entry;
}

void print_entry(struct RadioEntry *entry) {
    printf("\nStation: %s\n", entry->server_name);
    printf("Type: %s\n", entry->server_type);
    printf("Bitrate: %u\n", entry->bitrate);
    printf("Genre: %s\n", entry->genre);
    printf("URL: %s\n", entry->listen_url);
    if (entry->current_song[0]) {
        printf("Currently playing: %s\n", entry->current_song);
    }
}

void search_by_name(const char *term, struct IndexEntry *index, ULONG num_entries) {
    char lcterm[64];
    char lcname[64];
    UWORD i, j;
    BOOL found = FALSE;
    
    // Convert search term to lowercase
    for (i = 0; term[i] && i < 63; i++) {
        lcterm[i] = tolower(term[i]);
    }
    lcterm[i] = '\0';
    
    printf("Searching for name: %s\n", term);
    
    for (i = 0; i < num_entries; i++) {
        // Convert station name to lowercase
        for (j = 0; index[i].server_name[j] && j < 63; j++) {
            lcname[j] = tolower(index[i].server_name[j]);
        }
        lcname[j] = '\0';
        
        if (strstr(lcname, lcterm)) {
            struct RadioEntry *entry = load_entry((const unsigned char *)"radio.bin", index[i].offset);
            if (entry) {
                print_entry(entry);
                found = TRUE;
            }
        }
    }
    
    if (!found) {
        printf("No stations found matching: %s\n", term);
    }
}

void search_by_genre(const char *genre, const char *bin_filename, 
                    struct IndexEntry *index, ULONG num_entries) {
    struct RadioEntry *entry;
    BOOL found = FALSE;
    
    printf("Searching for genre: %s\n", genre);
    
    for (UWORD i = 0; i < num_entries; i++) {
        entry = load_entry((const unsigned char *)bin_filename, index[i].offset);
        if (entry && strstr(entry->genre, genre)) {
            print_entry(entry);
            found = TRUE;
        }
    }
    
    if (!found) {
        printf("No stations found with genre: %s\n", genre);
    }
}

void search_by_bitrate(UWORD min_bitrate, const char *bin_filename, 
                      struct IndexEntry *index, ULONG num_entries) {
    struct RadioEntry *entry;
    BOOL found = FALSE;
    
    printf("Searching for bitrate >= %u\n", min_bitrate);
    
    for (UWORD i = 0; i < num_entries; i++) {
        entry = load_entry((const unsigned char *)bin_filename, index[i].offset);
        if (entry && entry->bitrate >= min_bitrate) {
            print_entry(entry);
            found = TRUE;
        }
    }
    
    if (!found) {
        printf("No stations found with bitrate >= %u\n", min_bitrate);
    }
}

int main(int argc, char **argv) {
    if (argc != 3) {
        printf("Usage: %s <search_type> <search_term>\n", argv[0]);
        printf("Search types:\n");
        printf("  -n <name>     Search by station name\n");
        printf("  -g <genre>    Search by genre\n");
        printf("  -b <bitrate>  Search by minimum bitrate\n");
        return 1;
    }
    
    // First check if files exist
    BPTR test = Open("PROGDIR:radio.idx", MODE_OLDFILE);
    if (!test) {
        printf("Cannot find radio.idx file!\n");
        return 2;
    }
    Close(test);
    
    test = Open("PROGDIR:radio.bin", MODE_OLDFILE);
    if (!test) {
        printf("Cannot find radio.bin file!\n");
        return 2;
    }
    Close(test);
    
    struct IndexEntry *index = NULL;
    ULONG num_entries = 0;
    
    load_index((const unsigned char *)"PROGDIR:radio.idx", &index, &num_entries);
    if (!index) {
        printf("Failed to load index file!\n");
        return 2;
    }
    
    printf("Loaded %u entries from index\n", num_entries);
    
    if (strcmp(argv[1], "-n") == 0) {
        search_by_name(argv[2], index, num_entries);
    }
    else if (strcmp(argv[1], "-g") == 0) {
        search_by_genre(argv[2], "radio.bin", index, num_entries);
    }
    else if (strcmp(argv[1], "-b") == 0) {
        search_by_bitrate((UWORD)atoi(argv[2]), "radio.bin", index, num_entries);
    }
    else {
        printf("Invalid search type!\n");
    }
    
    if (index) {
        FreeMem(index, sizeof(struct IndexEntry) * num_entries);
    }
    
    return 0;
}