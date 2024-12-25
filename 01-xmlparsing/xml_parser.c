#include <exec/types.h>
#include <exec/memory.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

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

struct ParseState {
    char current_tag[32];
    UWORD tag_pos;
    UBYTE in_tag;
    UBYTE pad;
    char content[128];
    UWORD content_pos;
    UWORD pad2;
    struct RadioEntry *current_entry;
} ALIGN;

void init_parse_state(struct ParseState *state, struct RadioEntry *entry) {
    memset(state, 0, sizeof(struct ParseState));
    state->current_entry = entry;
}

void store_field(struct RadioEntry *entry, const char *tag, const char *content) {
    // Remove any whitespace at the beginning and end of content
    while (*content == ' ' || *content == '\n' || *content == '\r' || *content == '\t') content++;
    
    int len = strlen(content);
    while (len > 0 && (content[len-1] == ' ' || content[len-1] == '\n' || 
                      content[len-1] == '\r' || content[len-1] == '\t')) len--;
    
    if (len == 0) return;  // Skip empty content
    
    printf("Storing field: %s = %s\n", tag, content);
    
    if (strcmp(tag, "server_name") == 0) {
        strncpy(entry->server_name, content, sizeof(entry->server_name) - 1);
        entry->server_name[sizeof(entry->server_name) - 1] = '\0';
    } else if (strcmp(tag, "server_type") == 0) {
        strncpy(entry->server_type, content, sizeof(entry->server_type) - 1);
        entry->server_type[sizeof(entry->server_type) - 1] = '\0';
    } else if (strcmp(tag, "bitrate") == 0) {
        entry->bitrate = (UWORD)atoi(content);
    } else if (strcmp(tag, "samplerate") == 0) {
        entry->samplerate = (ULONG)atol(content);
    } else if (strcmp(tag, "channels") == 0) {
        entry->channels = (UBYTE)atoi(content);
    } else if (strcmp(tag, "listen_url") == 0) {
        strncpy(entry->listen_url, content, sizeof(entry->listen_url) - 1);
        entry->listen_url[sizeof(entry->listen_url) - 1] = '\0';
    } else if (strcmp(tag, "current_song") == 0) {
        strncpy(entry->current_song, content, sizeof(entry->current_song) - 1);
        entry->current_song[sizeof(entry->current_song) - 1] = '\0';
    } else if (strcmp(tag, "genre") == 0) {
        strncpy(entry->genre, content, sizeof(entry->genre) - 1);
        entry->genre[sizeof(entry->genre) - 1] = '\0';
    }
}

BOOL process_xml(BPTR file, struct RadioEntry *entries, struct IndexEntry *index, ULONG *num_entries) {
    struct ParseState state;
    UBYTE buffer[4096];
    LONG bytes_read;
    ULONG current_offset = 0;
    BOOL in_entry = FALSE;
    *num_entries = 0;
    
    struct RadioEntry current_entry;
    memset(&current_entry, 0, sizeof(struct RadioEntry));
    init_parse_state(&state, &current_entry);
    
    printf("Starting XML processing...\n");
    
    while ((bytes_read = Read(file, buffer, sizeof(buffer))) > 0) {
        printf("Read %ld bytes from XML\n", bytes_read);
        
        for (LONG i = 0; i < bytes_read; i++) {
            char c = (char)buffer[i];
            
            if (c == '<') {
                // Process content if we have any or wtf
                if (state.content_pos > 0) {
                    state.content[state.content_pos] = '\0';
                    if (in_entry) {
                        store_field(state.current_entry, state.current_tag, state.content);
                    }
                    state.content_pos = 0;
                }
                state.in_tag = 1;
                state.tag_pos = 0;
            }
            else if (c == '>') {
                state.current_tag[state.tag_pos] = '\0';
                state.in_tag = 0;
                
                if (strcmp(state.current_tag, "entry") == 0) {
                    in_entry = TRUE;
                    memset(&current_entry, 0, sizeof(struct RadioEntry));
                }
                else if (strcmp(state.current_tag, "/entry") == 0) {
                    if (in_entry && current_entry.server_name[0] != '\0') {
                        printf("Found complete entry: %s\n", current_entry.server_name);
                        
                        // Store the entry
                        CopyMem(&current_entry, &entries[*num_entries], sizeof(struct RadioEntry));
                        
                        // Update index
                        index[*num_entries].offset = *num_entries * sizeof(struct RadioEntry);
                        strncpy(index[*num_entries].server_name, current_entry.server_name, 63);
                        index[*num_entries].server_name[63] = '\0';
                        
                        (*num_entries)++;
                        printf("Total entries so far: %u\n", *num_entries);
                    }
                    in_entry = FALSE;
                }
            }
            else if (state.in_tag) {
                if (state.tag_pos < sizeof(state.current_tag) - 1) {
                    state.current_tag[state.tag_pos++] = c;
                }
            }
            else if (!state.in_tag && in_entry) {
                if (state.content_pos < sizeof(state.content) - 1) {
                    state.content[state.content_pos++] = c;
                }
            }
            
            current_offset++;
        }
    }
    
    printf("Finished XML processing. Found %u entries.\n", *num_entries);
    return *num_entries > 0;
}

BOOL save_binary_format(const char *filename, struct RadioEntry *entries, ULONG num_entries) {
    printf("Saving binary file: %s (entries: %u)\n", filename, num_entries);
    
    BPTR file = Open((CONST_STRPTR)filename, MODE_NEWFILE);
    if (!file) {
        printf("Failed to create binary file\n");
        return FALSE;
    }
    
    LONG bytes_to_write = sizeof(struct RadioEntry) * num_entries;
    LONG bytes_written = Write(file, entries, bytes_to_write);
    
    if (bytes_written != bytes_to_write) {
        printf("Failed to write binary file: wrote %ld of %ld bytes\n",
                bytes_written, bytes_to_write);
        Close(file);
        return FALSE;
    }
    
    printf("Successfully wrote %ld bytes to binary file\n", bytes_written);
    Close(file);
    return TRUE;
}

BOOL save_index(const char *filename, struct IndexEntry *index, ULONG num_entries) {
    printf("Saving index file: %s (entries: %u)\n", filename, num_entries);
    
    BPTR file = Open((CONST_STRPTR)filename, MODE_NEWFILE);
    if (!file) {
        printf("Failed to create index file\n");
        return FALSE;
    }
    
    LONG bytes_to_write = sizeof(struct IndexEntry) * num_entries;
    LONG bytes_written = Write(file, index, bytes_to_write);
    
    if (bytes_written != bytes_to_write) {
        printf("Failed to write index file: wrote %ld of %ld bytes\n",
                bytes_written, bytes_to_write);
        Close(file);
        return FALSE;
    }
    
    printf("Successfully wrote %ld bytes to index file\n", bytes_written);
    Close(file);
    return TRUE;
}

int main(int argc, char **argv) {
    if (argc != 2) {
        printf("Usage: %s <xml_file>\n", argv[0]);
        return 1;
    }
    
    printf("Allocating memory for entries and index...\n");
    
    struct RadioEntry *entries = AllocMem(sizeof(struct RadioEntry) * 1000, MEMF_CLEAR);
    struct IndexEntry *index = AllocMem(sizeof(struct IndexEntry) * 1000, MEMF_CLEAR);
    
    if (!entries || !index) {
        printf("Memory allocation failed!\n");
        return 2;
    }
    
    printf("Opening input file: %s\n", argv[1]);
    
    BPTR file = Open((CONST_STRPTR)argv[1], MODE_OLDFILE);
    if (!file) {
        printf("Could not open input file!\n");
        FreeMem(entries, sizeof(struct RadioEntry) * 1000);
        FreeMem(index, sizeof(struct IndexEntry) * 1000);
        return 3;
    }
    
    ULONG num_entries;
    BOOL success = process_xml(file, entries, index, &num_entries);
    Close(file);
    
    if (success && num_entries > 0) {
        printf("Processing completed. Saving %u entries...\n", num_entries);
        
        BOOL save_success = save_binary_format("radio.bin", entries, num_entries);
        if (!save_success) {
            printf("Failed to save binary file!\n");
        }
        
        save_success = save_index("radio.idx", index, num_entries);
        if (!save_success) {
            printf("Failed to save index file!\n");
        }
    } else {
        printf("No entries found or processing failed!\n");
    }
    
    FreeMem(entries, sizeof(struct RadioEntry) * 1000);
    FreeMem(index, sizeof(struct IndexEntry) * 1000);
    
    return 0;
}