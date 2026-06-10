#ifndef IPC_SERVER_HPP
#define IPC_SERVER_HPP

#include <string>
#include <thread>
#include <atomic>
#include "core/trie.hpp"
#include "core/inverted_index.hpp"

namespace FastSearch::OS {

class IPCServer {
public:
    IPCServer(Core::Trie& index, Core::InvertedIndex& inv_index);
    ~IPCServer();

    IPCServer(const IPCServer&) = delete;
    IPCServer& operator=(const IPCServer&) = delete;

    void start();
    void stop();

private:
    Core::Trie& shared_index;
    Core::InvertedIndex& shared_inv_index;
    std::atomic<bool> running;
    std::thread server_thread;

    void listen_loop();
};

} // namespace FastSearch::OS

#endif // IPC_SERVER_HPP