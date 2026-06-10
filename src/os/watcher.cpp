#include "os/watcher.hpp"
#include <windows.h>
#include <iostream>

namespace FastSearch::OS {

Watcher::Watcher(std::string target_dir, Core::Trie& index) 
    : directory(std::move(target_dir)), shared_index(index), running(false) {}

Watcher::~Watcher() {
    stop();
}

void Watcher::start() {
    if (!running) {
        running = true;
        background_thread = std::thread(&Watcher::watch_loop, this);
    }
}

void Watcher::stop() {
    if (running) {
        running = false;
        if (background_thread.joinable()) {
            // Detach allows the blocking Windows API to clean itself up 
            // naturally when the main program exits.
            background_thread.detach(); 
        }
    }
}

void Watcher::watch_loop() {
    // 1. Get a raw hook to the directory from the Windows Kernel
    HANDLE hDir = CreateFileA(
        directory.c_str(),
        FILE_LIST_DIRECTORY,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        NULL,
        OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS,
        NULL
    );

    if (hDir == INVALID_HANDLE_VALUE) {
        std::cerr << "[!] OS Watcher failed to attach to directory.\n";
        return;
    }

    char buffer[1024];
    DWORD bytes_returned;

    // 2. The Infinite Listening Loop
    while (running) {
        if (ReadDirectoryChangesW(
            hDir,
            &buffer,
            sizeof(buffer),
            TRUE, // Watch all subdirectories!
            FILE_NOTIFY_CHANGE_FILE_NAME, // Only wake up for new/deleted files
            &bytes_returned,
            NULL,
            NULL
        )) {
            // 3. Decode the event Windows just handed us
            FILE_NOTIFY_INFORMATION* fni = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(buffer);
            do {
                if (fni->Action == FILE_ACTION_ADDED) {
                    // Convert Windows Wide-Characters to standard C++ strings
                    std::wstring ws(fni->FileName, fni->FileNameLength / sizeof(WCHAR));
                    std::string filename(ws.begin(), ws.end());
                    std::string full_path = directory + "\\" + filename;
                    
                    // 4. Inject it into the Trie! (Our mutex makes this safe)
                    shared_index.insert(full_path);
                }
                
                // Move to the next event if multiple files were created at once
                fni = fni->NextEntryOffset ? reinterpret_cast<FILE_NOTIFY_INFORMATION*>(reinterpret_cast<BYTE*>(fni) + fni->NextEntryOffset) : nullptr;
            } while (fni);
        }
    }
    CloseHandle(hDir);
}

} // namespace FastSearch::OS