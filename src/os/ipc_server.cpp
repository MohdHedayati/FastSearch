#include "os/ipc_server.hpp"
#include <windows.h>
#include <iostream>
#include <sstream>
#include <chrono>

namespace FastSearch::OS {

IPCServer::IPCServer(Core::Trie& index, Core::InvertedIndex& inv_index) 
    : shared_index(index), shared_inv_index(inv_index), running(false) {}

IPCServer::~IPCServer() {
    stop();
}

void IPCServer::start() {
    if (!running) {
        running = true;
        server_thread = std::thread(&IPCServer::listen_loop, this);
    }
}

void IPCServer::stop() {
    if (running) {
        running = false;
        
        HANDLE hPipe = CreateFileA("\\\\.\\pipe\\FastSearchPipe", GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
        if (hPipe != INVALID_HANDLE_VALUE) CloseHandle(hPipe);
        
        if (server_thread.joinable()) {
            server_thread.join();
        }
    }
}

void IPCServer::listen_loop() {
    LPCSTR pipeName = "\\\\.\\pipe\\FastSearchPipe";

    while (running) {
        HANDLE hPipe = CreateNamedPipeA(
            pipeName,
            PIPE_ACCESS_DUPLEX,
            PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
            PIPE_UNLIMITED_INSTANCES,
            4096, 4096, 0, NULL);

        if (hPipe == INVALID_HANDLE_VALUE) {
            std::cerr << "[!] Failed to create IPC pipe.\n";
            Sleep(1000);
            continue;
        }

        bool connected = ConnectNamedPipe(hPipe, NULL) ? true : (GetLastError() == ERROR_PIPE_CONNECTED);

        if (connected && running) {
            char buffer[1024];
            DWORD bytesRead;

            if (ReadFile(hPipe, buffer, sizeof(buffer) - 1, &bytesRead, NULL) && bytesRead > 0) {
                buffer[bytesRead] = '\0';
                std::string query(buffer);

                auto start_search = std::chrono::high_resolution_clock::now();
                std::vector<std::string> results;

                // --- ROUTING LOGIC ---
                // If the client explicitly asked for a deep content scan
                if (query.rfind("deep:", 0) == 0) { 
                    std::string word = query.substr(5); // Strip "deep:" prefix
                    results = shared_inv_index.search_word(word);
                } else {
                    // Otherwise, do a standard fuzzy filename search (allowing 2 typos)
                    results = shared_index.search_fuzzy(query, 2);
                }
                
                auto end_search = std::chrono::high_resolution_clock::now();
                auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_search - start_search).count();

                std::ostringstream response;
                response << "Found " << results.size() << " results in " << duration << " microseconds.\n";
                
                int display_count = 0;
                for (const auto& res : results) {
                    response << " -> " << res << "\n";
                    if (++display_count >= 10) {
                        response << " ... and " << (results.size() - 10) << " more.\n";
                        break;
                    }
                }

                std::string response_str = response.str();
                DWORD bytesWritten;
                WriteFile(hPipe, response_str.c_str(), response_str.length(), &bytesWritten, NULL);
            }
        }
        
        FlushFileBuffers(hPipe);
        DisconnectNamedPipe(hPipe);
        CloseHandle(hPipe);
    }
}

} // namespace FastSearch::OS