# FastSearch Daemon

A multi-threaded, zero-latency local search engine and background daemon built entirely in C++17.

FastSearch bypasses the slow native Windows indexing by utilizing custom in-memory data structures (Prefix Tries and Inverted Indexes) and interacting directly with the Win32 Kernel to provide microsecond-level file and deep-content retrieval.

## 🚀 Architecture Highlights


*   **Dual Memory Engines:**
    * **Prefix Trie:** Powers the surface-level filename search, featuring a custom Dynamic Programming Levenshtein Automaton for typo tolerance (Fuzzy Matching).
    
    *   **Inverted Index:** Powers the deep-content search by tokenizing and mapping internal file text into a thread-safe std::set. Includes a strict 5MB chunk limit to prevent heap exhaustion.
        
*   **Win32 Kernel Watcher:** Hooks directly into ReadDirectoryChangesW to monitor the OS filesystem asynchronously, updating the search indexes in real-time without polling.
    
*   **Distributed IPC:** Utilizes Win32 Named Pipes (\\\\.\\pipe\\FastSearchPipe) to decouple the background daemon (`fast_searchd.exe`) from the lightweight CLI client (`fs.exe`).
    
*   **Thread Safety:** Highly concurrent architecture protected by `std::shared_mutex` (Read-Write locks) to prevent race conditions during massive filesystem crawls.
    
*   **Zero-Cost Cold Starts:** Uses custom binary serialization to dump the memory-mapped Trie to the SSD (.bin), allowing instant daemon reboots without re-crawling the disk.
    

⚡ Usage
-------

FastSearch operates as a decoupled system. The daemon runs silently in the background, and the client queries it via the command line.

### 1\. Start the Engine

Double-click `fast_searchd.exe` or run it from the terminal. It will load the binary index, parse your text files, attach to the Win32 Kernel, and listen for IPC connections.

### 2\. Query the Engine

Use the standalone client (`fs.exe`) from any terminal.

**Surface Search (Filename & Fuzzy Matching):** Returns results in ~40 microseconds.

```Bash
fs resume
# Also catches typos via Levenshtein DP:
fs resmu
``` 

**Deep Search (Internal Content Matching):** Bypasses filenames to search tokenized text inside .cpp, .json, .txt, etc.

```Bash   
fs deep localhost
fs deep kubernetes
```

🛠️ Build Instructions
----------------------

Built with CMake and heavily optimized for Windows/MinGW. Binaries are statically linked (-static-libgcc -static-libstdc++) for complete portability.

```Bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

🔮 V2 Roadmap (Future Optimizations)
------------------------------------

*   **Garbage Collection:** Transition the Inverted Index to handle FILE\_ACTION\_MODIFIED events with targeted memory deallocation.
    
*   **SSD Memory Mapping:** Replace std::unordered\_map with CreateFileMapping to offload the multi-gigabyte text index from RAM to disk while maintaining sub-millisecond query times.
    
*   **Lock-Free Structures:** Implement Hazard Pointers and atomic variables to replace std::shared\_mutex for zero-contention parallel crawling.
