#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include "downloader.h"

int main(int argc, char *argv[]) {
    // check args
    if (argc < 2) {
        printf("Usage: %s <URL> [output_file] [threads]\n", argv[0]);
        printf("Example: %s http://example.com/file.zip output.zip 4\n", argv[0]);
        return 1;
    }
    
    const char *url = argv[1];
    const char *output = (argc >= 3) ? argv[2] : "downloaded_file";
    int num_threads = (argc >= 4) ? atoi(argv[3]) : 4;
    
    if (num_threads < 1 || num_threads > 16) {
        fprintf(stderr, "Threads must be 1-16\n");
        return 1;
    }
    
    printf("=== Download Accelerator ===\n");
    printf("URL: %s\n", url);
    printf("Output: %s\n", output);
    printf("Threads: %d\n\n", num_threads);
    
    // get file size from server
    int supports_ranges = 0;
    long file_size = get_file_size(url, &supports_ranges);
    
    if (file_size <= 0) {
        fprintf(stderr, "Couldnt get file size\n");
        return 1;
    }
    
    printf("File size: %ld bytes (%.2f MB)\n", file_size, file_size / (1024.0 * 1024.0));
    printf("Range support: %s\n\n", supports_ranges ? "Yes" : "No");
    
    if (!supports_ranges) {
        printf("Warning: Server might not support ranges\n\n");
    }
    
    // create shared memory for IPC between parent and child
    SharedProgress* shared = mmap(NULL, sizeof(SharedProgress),
                                  PROT_READ | PROT_WRITE,
                                  MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (shared == MAP_FAILED) {
        fprintf(stderr, "mmap failed\n");
        return 1;
    }
    
    // initialize shared memory
    shared->total_downloaded = 0;
    shared->file_size = file_size;
    shared->done = 0;
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
    pthread_mutex_init(&shared->mutex, &attr);
    pthread_mutexattr_destroy(&attr);
    
    printf("Creating child process for downloading...\n");
    
    // fork - create child process
    pid_t pid = fork();
    
    if (pid < 0) {
        fprintf(stderr, "fork failed\n");
        munmap(shared, sizeof(SharedProgress));
        return 1;
    }
    
    if (pid == 0) {
        // CHILD PROCESS - does the downloading
        printf("Child process started (PID: %d)\n\n", getpid());
        
        long chunk_size = file_size / num_threads;
        printf("Downloading %d chunks in parallel...\n", num_threads);
        printf("Chunk size: %ld bytes (%.2f MB)\n\n", chunk_size, chunk_size / (1024.0 * 1024.0));
        
        int result = run_download(url, output, num_threads, file_size, shared);
        
        // child exits - dont destroy shared memory, parent needs it
        exit(result);
    }
    
    // PARENT PROCESS - shows progress
    printf("Parent process monitoring (PID: %d)\n", getpid());
    printf("Waiting for child (PID: %d) to download...\n\n", pid);
    
    // show progress while child downloads
    printf("Progress: ");
    fflush(stdout);
    
    while (1) {
        usleep(500000);  // wait 0.5 sec
        
        // read progress from shared memory
        pthread_mutex_lock(&shared->mutex);
        long current = shared->total_downloaded;
        int is_done = shared->done;
        pthread_mutex_unlock(&shared->mutex);
        
        float pct = (current * 100.0) / file_size;
        printf("\rProgress: %.1f%% (%ld / %ld bytes)", pct, current, file_size);
        fflush(stdout);
        
        if (is_done) break;
    }
    printf("\n\n");
    
    // wait for child to finish
    int status;
    waitpid(pid, &status, 0);
    
    if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
        printf("Child process completed successfully!\n");
    } else {
        printf("Child process failed!\n");
        pthread_mutex_destroy(&shared->mutex);
        munmap(shared, sizeof(SharedProgress));
        return 1;
    }
    
    // cleanup shared memory
    pthread_mutex_destroy(&shared->mutex);
    munmap(shared, sizeof(SharedProgress));
    
    printf("Done! Saved as: %s\n", output);
    
    // verify size matches
    FILE *f = fopen(output, "rb");
    if (f) {
        fseek(f, 0, SEEK_END);
        long final_size = ftell(f);
        fclose(f);
        
        printf("Expected: %ld bytes, Got: %ld bytes\n", file_size, final_size);
        if (final_size == file_size) {
            printf("File OK!\n");
        } else {
            printf("Size mismatch!\n");
        }
    }
    
    return 0;
}