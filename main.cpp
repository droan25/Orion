#include <iostream>
#include <string>
#include "CommandProcessor.h"
#include <cstdlib>
#ifdef __APPLE__
#include <cstdio>
#endif
#include <thread>
#include <atomic>
#include <chrono>
#include <fcntl.h>
#include <sys/select.h>
#include <fstream>
#include <algorithm> // for std::transform
#include <csignal>   // for kill
#include <unistd.h>  // for getpid
#include "StorageHelper.h"
#include <limits> // Required for std::numeric_limits


std::atomic<bool> interruptSpeech(false);

// Function to check if user pressed Enter (non-blocking)
void pollStdin() {
    int stdin_fd = fileno(stdin);
    struct timeval tv = {0, 0};
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(stdin_fd, &fds);
    if (select(stdin_fd + 1, &fds, NULL, NULL, &tv) > 0) {
        char c = getchar();
        if (c == '\n') {
            interruptSpeech = true;
        }
    }
}


std::string getVoiceInput() {
    #ifdef __APPLE__
        std::cout << "(Say your command after the beep...)\n";
        system("say 'Listening'");
        
        // Clear any existing input buffer
        std::cin.clear();
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        
        // Use macOS dictation with a different approach
        system("osascript -e 'tell application \"System Events\" to keystroke \"fn\"' > /dev/null 2>&1");
        
        // Wait a moment for dictation to activate
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    
        std::cout << "Speak and press RETURN when done...\n";
        std::string result;
        std::getline(std::cin, result);
        
        // Clean up the result - remove any "fn" artifacts and common dictation artifacts
        if (result.find("fn") == 0) {
            result = result.substr(2);
        }
        // Remove common dictation artifacts
        std::string cleaned = result;
        cleaned.erase(0, cleaned.find_first_not_of(" \n\r\t"));
        cleaned.erase(cleaned.find_last_not_of(" \n\r\t") + 1);
        
        // Remove any remaining "fn" or dictation artifacts
        if (cleaned.find("fn") != std::string::npos) {
            cleaned = cleaned.substr(cleaned.find("fn") + 2);
            cleaned.erase(0, cleaned.find_first_not_of(" \n\r\t"));
        }
        
        return cleaned;
    #else
        return "";
    #endif
}

void speak(const std::string& text) {
    #ifdef __APPLE__
        std::string cmd = "say \"" + text + "\" & echo $! > /tmp/saypid";
        system(cmd.c_str());
    #endif
}


void stopSpeaking() {
    #ifdef __APPLE__
        std::ifstream pidFile("/tmp/saypid");
        pid_t pid;
        if (pidFile >> pid) {
            std::string killCmd = "kill " + std::to_string(pid);
            system(killCmd.c_str());
        }
    #endif
    }
    

int main() {

    StorageHelper::initDatabase();
    StorageHelper::initialize();  // Add this
    
    CommandProcessor jarvis;

    std::string input;
    std::cout << "Welcome to Orion! Type 'exit' to quit.\n";
    std::cout << "Choose input mode: (1) Text  (2) Voice\n> ";
    int mode = 1;
    std::cin >> mode;
    std::cin.ignore();
    while (true) {
        if (mode == 2) {
            input = getVoiceInput();
            std::cout << "You (voice): " << input << std::endl;
            
            // Skip processing if input is empty or just whitespace
            if (input.empty() || input.find_first_not_of(" \n\r\t") == std::string::npos) {
                std::cout << "No command detected. Please try again.\n";
                continue;
            }
        } else {
            std::cout << "You: ";
            std::getline(std::cin, input);
        }
        std::string lower = input;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
        if (lower == "exit" || lower == "orion stop" || lower == "stop listening") {
        #ifdef __APPLE__
        // Try killing any running "say" processes (Mac TTS)
            system("killall say > /dev/null 2>&1");
        #endif
            speak("Orion is shutting down. Goodbye!");
            break;
        }



        std::string response = jarvis.processCommand(input);
        StorageHelper::logHistory(input, response);

        std::cout << "Orion: " << response << std::endl;

        interruptSpeech = false;
        speak(response);

        // Poll for Enter key every 100ms while speaking
        for (int i = 0; i < 100; ++i) {  // 100 × 100ms = 10 sec max speech
            pollStdin();
            if (interruptSpeech) {
                stopSpeaking();
                std::cout << "(Speech interrupted)\n";
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

    }
    std::cout << "Goodbye!\n";
    return 0;
}