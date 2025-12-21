# Accelerated Downloader

A multi-threaded download accelerator written in C that uses parallel connections to speed up file downloads. The program uses fork() to create a child process for downloading and displays real-time progress in the parent process using shared memory for IPC.

## Features

- **Multi-threaded downloads**: Split files into chunks and download in parallel
- **Process-based architecture**: Uses fork() with shared memory for IPC
- **Real-time progress tracking**: Parent process monitors download progress
- **HTTP range request support**: Automatically detects if server supports partial downloads
- **File integrity verification**: Compares final file size with expected size

## Requirements

- GCC compiler
- libcurl development libraries
- pthread support
- POSIX-compliant system (Linux/Unix)

## Installation

Install dependencies (Ubuntu/Debian):
```bash
sudo apt-get install build-essential libcurl4-openssl-dev
```

Build the project:
```bash
make
```

## Usage

```bash
./downloader <URL> [output_file] [threads]
```

### Parameters

- `URL`: The file URL to download (required)
- `output_file`: Output filename (optional, default: "downloaded_file")
- `threads`: Number of parallel threads (optional, default: 4, max: 16)

## Examples

### Real File Download (Recommended for Testing)
```bash
# Download 7-Zip (1.5MB, real compressed file)
./downloader "https://www.7-zip.org/a/7z2408-linux-x64.tar.xz" 7zip.tar.xz 5

# Verify integrity
tar -tf 7zip.tar.xz
```

### Speed Test Files (Binary Data - Not Real Archives)

**Note:** These Tele2 URLs provide binary test data for bandwidth testing. They are NOT valid ZIP files and cannot be extracted. Use them only to test download speed.

#### Quick Test (1MB)
```bash
./downloader "http://speedtest.tele2.net/1MB.zip" test_1mb.bin 5
```

#### Small File (5MB)
```bash
./downloader "http://speedtest.tele2.net/5MB.zip" test_5mb.bin 5
```

#### Medium File (10MB)
```bash
./downloader "http://speedtest.tele2.net/10MB.zip" test_10mb.bin 5
```

#### Large File (20MB)
```bash
./downloader "http://speedtest.tele2.net/20MB.zip" test_20mb.bin 8
```

#### Very Large File (100MB)
```bash
./downloader "http://speedtest.tele2.net/100MB.zip" test_100mb.bin 10
```

#### Huge File (512MB)
```bash
./downloader "http://speedtest.tele2.net/512MB.zip" test_512mb.bin 12
```

#### Maximum File (1GB)
```bash
./downloader "http://speedtest.tele2.net/1GB.zip" test_1gb.bin 16
```

## Performance Tips

- Use more threads (8-16) for larger files
- Use fewer threads (2-4) for smaller files or slower connections
- Servers must support HTTP range requests for parallel downloads
- Network speed and server capacity affect performance

## Build Commands

```bash
# Build project
make

# Clean build files
make clean

# Rebuild from scratch
make rebuild

# Run default test
make test
```

## How It Works

1. **Parent Process**: Sends HEAD request to get file size and check range support
2. **Fork**: Creates child process for downloading
3. **Child Process**: 
   - Splits file into chunks
   - Creates multiple threads to download chunks in parallel
   - Updates shared memory with progress
4. **Parent Process**: Monitors progress and displays percentage
5. **Merge**: Child process merges all chunks into final file
6. **Verification**: Checks if downloaded file size matches expected size

## Architecture

```
Parent Process (PID: X)
├── Shared Memory (mmap)
│   ├── total_downloaded
│   ├── file_size
│   ├── done flag
│   └── mutex
└── Child Process (PID: Y)
    ├── Thread 0 → Chunk 0
    ├── Thread 1 → Chunk 1
    ├── Thread 2 → Chunk 2
    ├── Thread 3 → Chunk 3
    └── Thread N → Chunk N
```

## File Structure

- `main.c`: Entry point, process management, progress display
- `downloader.c`: Download logic, thread workers, chunk merging
- `downloader.h`: Header file with struct definitions and function prototypes
- `Makefile`: Build configuration

## Troubleshooting

### Server doesn't support ranges
If you see "Range support: No", the server may not support parallel downloads. The program will still work but won't be accelerated.

### Size mismatch error
This usually indicates:
- Network interruption during download
- Server sent incorrect data
- File was modified during download

### Compilation warnings
The `usleep` warning is harmless and doesn't affect functionality.

## License

MIT License - Feel free to use and modify.

## Author

Created as a demonstration of multi-threaded downloading with process-based IPC.
