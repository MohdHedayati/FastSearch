#include <windows.h>
#include <iostream>
#include <string>

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cout << "FastSearch Engine\n";
        std::cout << "  Filename Search: fs <query>\n";
        std::cout << "  Content Search:  fs deep <word>\n";
        return 1;
    }

    std::string query = "";
    
    // Check if the user wants a deep content scan
    if (std::string(argv[1]) == "deep" && argc >= 3) {
        query = "deep:" + std::string(argv[2]); 
    } else {
        // Standard filename search
        query = argv[1];
        for (int i = 2; i < argc; ++i) {
            query += " ";
            query += argv[i];
        }
    }

    char buffer[4096];
    DWORD bytesRead;

    bool success = CallNamedPipeA(
        "\\\\.\\pipe\\FastSearchPipe", 
        (LPVOID)query.c_str(),         
        query.length(),                
        buffer,                        
        sizeof(buffer) - 1,            
        &bytesRead,                    
        NMPWAIT_USE_DEFAULT_WAIT       
    );

    if (success) {
        buffer[bytesRead] = '\0'; 
        std::cout << buffer;      
    } else {
        std::cerr << "[!] Error: Could not connect to FastSearch Daemon.\n";
        std::cerr << "    Is fast_searchd.exe running in the background?\n";
    }

    return 0;
}