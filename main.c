#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
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
    
    // figure out chunk sizes
    long chunk_size = file_size / num_threads;
    
    printf("Downloading %d chunks in parallel...\n", num_threads);
    printf("Chunk size: %ld bytes (%.2f MB)\n\n", chunk_size, chunk_size / (1024.0 * 1024.0));
    
    // shared progress counter - all threads update this
    long total_downloaded = 0;
    pthread_mutex_t mutex;
    pthread_mutex_init(&mutex, NULL);
    
    // allocate memory for threads
    pthread_t* tids = malloc(num_threads * sizeof(pthread_t));
    ChunkInfo* chunks = malloc(num_threads * sizeof(ChunkInfo));
    
    if (!tids || !chunks) {
        fprintf(stderr, "malloc failed\n");
        return 1;
    }
    
    // create all download threads
    for (int i = 0; i < num_threads; i++) {
        long start = i * chunk_size;
        long end = (i == num_threads - 1) ? file_size - 1 : (start + chunk_size - 1);
        
        chunks[i].url = url;
        chunks[i].start_byte = start;
        chunks[i].end_byte = end;
        chunks[i].chunk_id = i;
        chunks[i].total_downloaded = &total_downloaded;
        chunks[i].mutex = &mutex;
        
        if (pthread_create(&tids[i], NULL, download_worker, &chunks[i]) != 0) {
            fprintf(stderr, "Failed to create thread %d\n", i);
            return 1;
        }
    }
    
    // show progress while threads are working
    printf("Progress: ");
    fflush(stdout);
    int all_done = 0;
    while (!all_done) {
        usleep(500000);  // wait 0.5 sec
        
        // safely read progress
        pthread_mutex_lock(&mutex);
        long current = total_downloaded;
        pthread_mutex_unlock(&mutex);
        
        float pct = (current * 100.0) / file_size;
        printf("\rProgress: %.1f%% (%ld / %ld bytes)", pct, current, file_size);
        fflush(stdout);
        
        if (current >= file_size) all_done = 1;
    }
    printf("\n");
    
    // wait for threads to complete
    printf("Waiting for threads to finish...\n");
    for (int i = 0; i < num_threads; i++) {
        void* ret;
        pthread_join(tids[i], &ret);
        if ((long)ret != 0) {
            fprintf(stderr, "Thread %d had errors\n", i);
        }
    }
    
    // cleanup
    pthread_mutex_destroy(&mutex);
    free(tids);
    free(chunks);
    
    printf("\nMerging chunks...\n");
    
    // put all parts together
    if (merge_chunks(num_threads, output) != 0) {
        fprintf(stderr, "Merge failed\n");
        return 1;
    }
    
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