#ifndef DOWNLOADER_H
#define DOWNLOADER_H

#include <stdio.h>
#include <curl/curl.h>
#include <pthread.h>

// shared memory struct for IPC
typedef struct {
    long total_downloaded;
    long file_size;
    int done;
    pthread_mutex_t mutex;
} SharedProgress;

// info for each thread's chunk
typedef struct {
    const char *url;
    long start_byte;
    long end_byte;
    int chunk_id;
    long* total_downloaded;
    pthread_mutex_t* mutex;
} ChunkInfo;

// file info
long get_file_size(const char *url, int *supports_ranges);

// download functions
int download_chunk(const char *url, long start, long end, const char *filename);
int download_chunk_with_progress(const char *url, long start, long end, const char *filename, long* progress, pthread_mutex_t* mutex);

// file operations
int merge_chunks(int num_chunks, const char *output_file);

// thread worker
void* download_worker(void* arg);

// download manager for child process
int run_download(const char* url, const char* output, int num_threads, 
                 long file_size, SharedProgress* shared);

#endif
