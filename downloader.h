#ifndef DOWNLOADER_H
#define DOWNLOADER_H

#include <stdio.h>
#include <curl/curl.h>
#include <pthread.h>

// info for each thread's chunk
typedef struct {
    const char *url;
    long start_byte;
    long end_byte;
    int chunk_id;
    long* total_downloaded;  // shared counter
    pthread_mutex_t* mutex;  // protect the counter
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

#endif
