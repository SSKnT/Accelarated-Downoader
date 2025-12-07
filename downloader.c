#include "downloader.h"
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

// for progress tracking
typedef struct {
    long* total_downloaded;
    pthread_mutex_t* mutex;
} ProgressData;

// discard callback - just throw away data
size_t discard_callback(void *ptr, size_t size, size_t nmemb, void *data) {
    (void)ptr;
    (void)data;
    return size * nmemb;
}

// basic write callback
size_t write_callback(void *ptr, size_t size, size_t nmemb, FILE *stream) {
    size_t written = fwrite(ptr, size, nmemb, stream);
    return written;
}

// write callback that tracks progress
size_t write_callback_progress(void *ptr, size_t size, size_t nmemb, void* userdata) {
    void** data = (void**)userdata;
    FILE* fp = (FILE*)data[0];
    ProgressData* pdata = (ProgressData*)data[1];
    
    size_t written = fwrite(ptr, size, nmemb, fp);
    
    // update shared counter safely
    if (pdata && pdata->mutex && pdata->total_downloaded) {
        pthread_mutex_lock(pdata->mutex);
        *(pdata->total_downloaded) += written;
        pthread_mutex_unlock(pdata->mutex);
    }
    
    return written;
}

// get file size and check range support
long get_file_size(const char *url, int *supports_ranges) {
    CURL *curl;
    CURLcode res;
    curl_off_t size = 0;
    long code = 0;
    
    curl = curl_easy_init();
    if (!curl) return -1;
    
    // try range request first
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_FAILONERROR, 0L);
    curl_easy_setopt(curl, CURLOPT_NOBODY, 0L);
    curl_easy_setopt(curl, CURLOPT_HEADER, 1L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, discard_callback);
    curl_easy_setopt(curl, CURLOPT_RANGE, "0-0");
    
    res = curl_easy_perform(curl);
    
    if (res == CURLE_OK) {
        curl_easy_getinfo(curl, CURLINFO_CONTENT_LENGTH_DOWNLOAD_T, &size);
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
        
        // 206 = partial, 200 = ok
        *supports_ranges = (code == 206 || code == 200) ? 1 : 0;
        
        // if got 1 byte or nothing, need real size
        if (size <= 1 || size == 0) {
            curl_easy_cleanup(curl);
            
            curl = curl_easy_init();
            curl_easy_setopt(curl, CURLOPT_URL, url);
            curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
            curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
            curl_easy_setopt(curl, CURLOPT_FAILONERROR, 0L);
            
            res = curl_easy_perform(curl);
            if (res == CURLE_OK) {
                curl_easy_getinfo(curl, CURLINFO_CONTENT_LENGTH_DOWNLOAD_T, &size);
            }
        }
    } else {
        fprintf(stderr, "Error getting file info: %s\n", curl_easy_strerror(res));
        curl_easy_cleanup(curl);
        return -1;
    }
    
    curl_easy_cleanup(curl);
    return (long)size;
}

// download byte range with optional progress
int download_chunk_with_progress(const char *url, long start, long end, 
                                   const char *filename, long* progress, 
                                   pthread_mutex_t* mutex) {
    CURL *curl;
    FILE *fp;
    CURLcode res;
    char range[64];
    
    curl = curl_easy_init();
    if (!curl) {
        fprintf(stderr, "curl init failed\n");
        return -1;
    }
    
    fp = fopen(filename, "wb");
    if (!fp) {
        fprintf(stderr, "Cant open: %s\n", filename);
        curl_easy_cleanup(curl);
        return -1;
    }
    
    snprintf(range, sizeof(range), "%ld-%ld", start, end);
    
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_RANGE, range);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_FAILONERROR, 0L);
    
    // if tracking progress, use special callback
    void* userdata[2];
    ProgressData pdata;
    
    if (progress && mutex) {
        pdata.total_downloaded = progress;
        pdata.mutex = mutex;
        userdata[0] = fp;
        userdata[1] = &pdata;
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback_progress);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, userdata);
    } else {
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
    }
    
    res = curl_easy_perform(curl);
    
    fclose(fp);
    curl_easy_cleanup(curl);
    
    if (res != CURLE_OK) {
        fprintf(stderr, "Download failed: %s\n", curl_easy_strerror(res));
        return -1;
    }
    
    return 0;
}

// simple wrapper - no progress tracking
int download_chunk(const char *url, long start, long end, const char *filename) {
    return download_chunk_with_progress(url, start, end, filename, NULL, NULL);
}

// merge all parts into final file
int merge_chunks(int num_chunks, const char *output_file) {
    FILE *out = fopen(output_file, "wb");
    if (!out) {
        fprintf(stderr, "Cant open output: %s\n", output_file);
        return -1;
    }
    
    char part_name[256];
    char buffer[8192];
    size_t bytes;
    
    // read each part and write to output
    for (int i = 0; i < num_chunks; i++) {
        snprintf(part_name, sizeof(part_name), "part_%d.tmp", i);
        
        FILE *part = fopen(part_name, "rb");
        if (!part) {
            fprintf(stderr, "Cant open part: %s\n", part_name);
            fclose(out);
            return -1;
        }
        
        // copy data
        while ((bytes = fread(buffer, 1, sizeof(buffer), part)) > 0) {
            fwrite(buffer, 1, bytes, out);
        }
        
        fclose(part);
        remove(part_name);  // delete temp file
    }
    
    fclose(out);
    return 0;
}

// worker function for each thread
void* download_worker(void* arg) {
    ChunkInfo* info = (ChunkInfo*)arg;
    char part_file[256];
    
    snprintf(part_file, sizeof(part_file), "part_%d.tmp", info->chunk_id);
    
    printf("Thread %d: downloading bytes %ld-%ld\n", 
           info->chunk_id, info->start_byte, info->end_byte);
    
    if (download_chunk_with_progress(info->url, info->start_byte, info->end_byte, 
                                      part_file, info->total_downloaded, info->mutex) != 0) {
        fprintf(stderr, "Thread %d failed\n", info->chunk_id);
        pthread_exit((void*)-1);
    }
    
    printf("Thread %d: done\n", info->chunk_id);
    pthread_exit((void*)0);
}
