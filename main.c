#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdbool.h>

#ifdef _WIN32
#include <io.h>
#define ftruncate _chsize
#else
#include <unistd.h>
#endif

#define BUFFER_SIZE 4096

typedef struct {
    FILE* original;
    FILE* temp;
} FileHandles;

void print_usage(const char* program_name) {
    fprintf(stderr, "Usage: %s <file path> <characters to delete>\n", program_name);
}

void close_files(FileHandles* handles) {
    if (handles->original) fclose(handles->original);
    if (handles->temp) fclose(handles->temp);
}

void handle_error(const char* message, FileHandles* handles) {
    perror(message);
    close_files(handles);
    exit(EXIT_FAILURE);
}

FileHandles open_files(const char* filename) {
    FileHandles handles = {NULL, NULL};
    handles.original = fopen(filename, "r+");
    if (handles.original == NULL) {
        handle_error("Error opening original file", &handles);
    }
    handles.temp = tmpfile();
    if (handles.temp == NULL) {
        handle_error("Error creating temporary file", &handles);
    }
    return handles;
}

void process_file(FileHandles* handles, const char* targets) {
    int current_character;
    while ((current_character = fgetc(handles->original)) != EOF) {
        if (strchr(targets, current_character) == NULL) {
            if (fputc(current_character, handles->temp) == EOF) {
                handle_error("Error writing to temporary file", handles);
            }
        }
    }
    if (ferror(handles->original)) {
        handle_error("Error reading from original file", handles);
    }
}

long get_file_size(FILE* file) {
    if (fseek(file, 0, SEEK_END) != 0) {
        return -1;
    }
    long size = ftell(file);
    if (size == -1) {
        return -1;
    }
    if (fseek(file, 0, SEEK_SET) != 0) {
        return -1;
    }
    return size;
}

bool copy_file_contents(FILE* src, FILE* dest) {
    char buffer[BUFFER_SIZE];
    size_t bytesRead;
    while ((bytesRead = fread(buffer, 1, sizeof(buffer), src)) > 0) {
        if (fwrite(buffer, 1, bytesRead, dest) != bytesRead) {
            return false;
        }
    }
    return !ferror(src);
}

void replace_original_file(FileHandles* handles) {
    long temp_size = get_file_size(handles->temp);
    if (temp_size == -1) {
        handle_error("Error getting temporary file size", handles);
    }

    if (fseek(handles->original, 0, SEEK_SET) != 0 || fseek(handles->temp, 0, SEEK_SET) != 0) {
        handle_error("Error rewinding files", handles);
    }

    if (!copy_file_contents(handles->temp, handles->original)) {
        handle_error("Error copying file contents", handles);
    }

    if (fflush(handles->original) != 0) {
        handle_error("Error flushing original file", handles);
    }

    int fd = fileno(handles->original);
    if (ftruncate(fd, temp_size) != 0) {
        handle_error("Error truncating file", handles);
    }
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    FileHandles handles = open_files(argv[1]);
    process_file(&handles, argv[2]);
    replace_original_file(&handles);
    close_files(&handles);

    return EXIT_SUCCESS;
}