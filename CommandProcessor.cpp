#include "CommandProcessor.h"
#include <ctime>
#include <cstdlib>
#include <sstream>
#include <algorithm>
#ifdef __APPLE__
#include <unistd.h>
#endif
#include <cstdio>
#include <memory>
#include <array>
#include <curl/curl.h>
#include <sstream>
#include <iostream>
#include <string>
#include <regex>
#include "StorageHelper.h"



// --- Llama.cpp subprocess integration ---
std::string askLlama(const std::string& prompt) {
    std::string formattedPrompt = "<|user|>\nYou are Orion, a helpful AI assistant. Keep responses concise and relevant. Answer in 1-2 sentences maximum.\n\nUser: " + prompt + "\n<|assistant|>";

    std::string cmd = "/path/to/llama.cpp/build/bin/llama-run"
        " --ngl 999"
        " /path/to/llama.cpp/models/tinyllama/tinyllama-1.1b-chat-v1.0.Q4_K_M.gguf"
        " \"" + formattedPrompt + "\"";

    std::array<char, 256> buffer;
    std::string result;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);
    if (!pipe) return "Failed to run llama-run";

    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }

    auto start = result.find("<|assistant|>");
    if (start != std::string::npos) {
        start += std::string("<|assistant|>").length();
        auto end = result.find("\n<", start);
        std::string answer = (end != std::string::npos)
            ? result.substr(start, end - start)
            : result.substr(start);
        answer.erase(0, answer.find_first_not_of(" \n\r\t"));
        answer.erase(answer.find_last_not_of(" \n\r\t") + 1);
        return answer;
    }

    // fallback
    std::istringstream iss(result);
    std::string line, last;
    while (std::getline(iss, line)) {
        if (!line.empty()) last = line;
    }
    return last.empty() ? "No response from Llama" : last;
}


// --- End Llama.cpp integration ---

// --- Sample Plugin: PlayMusicPlugin ---
class PlayMusicPlugin : public Plugin {
public:
    std::string name() const override { return "play_music"; }
    std::string handle(const std::string& command) override {
        if (command.rfind("play music ", 0) == 0) {
            std::string file = command.substr(11);
#ifdef __APPLE__
            std::string cmd = "open '" + file + "'";
#else
            std::string cmd = "xdg-open '" + file + "'";
#endif
            system(cmd.c_str());
            return "Playing music: " + file;
        }
        return "";
    }
};
// --- End Sample Plugin ---

CommandProcessor::CommandProcessor() {
    addPlugin(std::make_shared<PlayMusicPlugin>());
}

void CommandProcessor::addPlugin(std::shared_ptr<Plugin> plugin) {
    plugins[plugin->name()] = plugin;
}

static size_t WriteCallback(char* ptr, size_t size, size_t nmemb, void* userdata) {
    if (!userdata) return 0;
    std::ostringstream* stream = static_cast<std::ostringstream*>(userdata);
    size_t count = size * nmemb;
    stream->write(ptr, count);
    return count;
}

std::string getYoutubeTopVideoUrl(const std::string& query) {
    std::string apiKey = "YOUR_YOUTUBE_API_KEY_HERE";
    std::string apiUrl = "https://www.googleapis.com/youtube/v3/search?part=snippet&type=video&maxResults=1&q=" + 
                         std::regex_replace(query, std::regex(" "), "+") + 
                         "&key=" + apiKey;

    CURL* curl = curl_easy_init();
    if (!curl) return "Failed to initialize CURL.";

    std::ostringstream responseStream;
    curl_easy_setopt(curl, CURLOPT_URL, apiUrl.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseStream);
    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        return "Failed to contact YouTube.";
    }

    std::string json = responseStream.str();

    std::smatch match;
    std::regex rgx("\"videoId\"\\s*:\\s*\"([^\"]+)\"");
    if (std::regex_search(json, match, rgx)) {
        std::string videoId = match[1];
        return "https://www.youtube.com/watch?v=" + videoId + "&autoplay=1";
    }
    return "No video found.";
}


std::string getWeather(const std::string& city) {
    std::string apiKey = "YOUR_OPENWEATHERMAP_API_KEY_HERE";
    std::string url = "https://api.openweathermap.org/data/2.5/weather?q=" + 
                      std::regex_replace(city, std::regex(" "), "%20") +
                      "&appid=" + apiKey + "&units=metric";

    CURL* curl = curl_easy_init();
    if (!curl) return "Failed to initialize CURL.";

    std::ostringstream responseStream;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);

    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseStream);

    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        return "Failed to fetch weather data.";
    }

    std::string json = responseStream.str();

    // Defensive parsing
    try {
        auto tempStart = json.find("\"temp\":");
        if (tempStart == std::string::npos) throw std::runtime_error("Temp missing");
        size_t tempEnd = json.find(",", tempStart);
        float temp = std::stof(json.substr(tempStart + 7, tempEnd - (tempStart + 7)));

        auto descStart = json.find("\"description\":\"");
        if (descStart == std::string::npos) throw std::runtime_error("Desc missing");
        size_t descEnd = json.find("\"", descStart + 15);
        std::string desc = json.substr(descStart + 15, descEnd - (descStart + 15));

        std::ostringstream result;
        result << "The current weather in " << city << " is " << temp << "°C with " << desc << ".";
        return result.str();
    } catch (...) {
        return "Weather data parsing failed.";
    }
}

std::string CommandProcessor::processCommand(const std::string& command) {
    std::string lower = command;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    // 0. Try opening Folder
    std::string folder = handleFolderOpen(command);
    if (!folder.empty()) return folder;


    // 1. Try system control
    std::string sys = handleSystemControl(command);
    if (!sys.empty()) return sys;

    // 2. Try volume control
    std::string vol = handleVolumeControl(command);
    if (!vol.empty()) return vol;

    // 3. Open website or search online using regex
    std::smatch match;
    std::regex pattern(R"(open (?:website|site)? ?(.+))", std::regex_constants::icase);
    if (std::regex_search(command, match, pattern)) {
        std::string site = match[1];
        site.erase(0, site.find_first_not_of(" ")); // remove leading space

        std::string url;
        if (site.find("http") == 0 || site.find("www.") == 0 || site.find(".com") != std::string::npos || site.find(".org") != std::string::npos) {
            url = (site.find("http") == 0) ? site : "https://" + site;
        } else {
            std::string query = std::regex_replace(site, std::regex(" "), "+");
            url = "https://www.google.com/search?q=" + query;
        }

    #ifdef __APPLE__
        std::string cmd = "open \"" + url + "\"";
    #else
        std::string cmd = "xdg-open \"" + url + "\"";
    #endif
        system(cmd.c_str());
        return "Opening " + url;
    }

    // 4. Try reminders
    std::string rem = handleReminders(command);
    if (!rem.empty()) return rem;

    // 5. Try file search
    std::string files = handleFileSearch(command);
    if (!files.empty()) return files;

    // 6. Plugins
    for (auto& [name, plugin] : plugins) {
        std::string result = plugin->handle(command);
        if (!result.empty()) return result;
    }

    // 7. Weather
    if (lower.find("weather") != std::string::npos && lower.find("in") != std::string::npos) {
        size_t weatherPos = lower.find("weather");
        size_t inPos = lower.find("in", weatherPos);
        if (inPos != std::string::npos) {
            std::string city = command.substr(inPos + 3);
            // Clean up the city name - remove extra words like "today", "now", etc.
            std::vector<std::string> wordsToRemove = {"today", "now", "please", "tell me", "what is"};
            for (const auto& word : wordsToRemove) {
                size_t pos = city.find(word);
                if (pos != std::string::npos) {
                    city = city.substr(0, pos);
                }
            }
            // Clean up whitespace
            city.erase(0, city.find_first_not_of(" \n\r\t"));
            city.erase(city.find_last_not_of(" \n\r\t") + 1);
            return getWeather(city);
        }
    }

    // 8. YouTube
    if (lower.find("play ") == 0 || lower.find("play music") != std::string::npos) {
        std::string song;
        if (lower.find("play music") != std::string::npos) {
            song = command.substr(lower.find("music") + 6);
        } else {
            song = command.substr(5);
        }
        
        // Clean up the song name
        song.erase(0, song.find_first_not_of(" \n\r\t"));
        song.erase(song.find_last_not_of(" \n\r\t") + 1);
        
        if (!song.empty()) {
            std::string videoUrl = getYoutubeTopVideoUrl(song);
            if (videoUrl.find("http") == 0) {
#ifdef __APPLE__
                system(("open \"" + videoUrl + "\"").c_str());
#else
                system(("xdg-open \"" + videoUrl + "\"").c_str());
#endif
                return "Playing " + song + " on YouTube.";
            } else {
                return videoUrl;
            }
        } else {
            return "Please specify what you'd like to play.";
        }
    }

    // 9. Date and Time
    if (lower.find("time") != std::string::npos) {
        time_t now = time(0);
        struct tm* localtm = localtime(&now);
        char buffer[80];
        strftime(buffer, sizeof(buffer), "The current time is %I:%M %p.", localtm);
        return buffer;
    }

    if (lower.find("date") != std::string::npos) {
        time_t now = time(0);
        struct tm* localtm = localtime(&now);
        char buffer[80];
        strftime(buffer, sizeof(buffer), "Today's date is %A, %B %d, %Y.", localtm);
        return buffer;
    }

    // 10. Simple Calculator
    std::smatch calcMatch;
    std::regex calcRegex(R"((what('| i)?s|calculate)\s+([\d\s\+\-\*\/\%\.\(\)]+))", std::regex::icase);
    if (std::regex_search(command, calcMatch, calcRegex)) {
        std::string expression = calcMatch[3];

        // Clean expression to keep only valid characters
        expression.erase(std::remove_if(expression.begin(), expression.end(), [](char c) {
            return !isdigit(c) && std::string("+-*/%.() ").find(c) == std::string::npos;
        }), expression.end());

        std::string calcCmd = "echo '" + expression + "' | bc -l";
        FILE* pipe = popen(calcCmd.c_str(), "r");
        if (!pipe) return "Calculation failed.";

        char buffer[128];
        std::string result;
        while (fgets(buffer, sizeof(buffer), pipe)) result += buffer;
        pclose(pipe);

        // Trim whitespace
        result.erase(0, result.find_first_not_of(" \n\r\t"));
        result.erase(result.find_last_not_of(" \n\r\t") + 1);

        // Remove trailing zeros and decimal if unnecessary
        if (result.find('.') != std::string::npos) {
            while (!result.empty() && result.back() == '0') result.pop_back();
            if (!result.empty() && result.back() == '.') result.pop_back();
        }

        // Add leading 0 if result starts with '.'
        if (!result.empty() && result[0] == '.') result = "0" + result;

        return "Answer is: " + result;
    }

    // 11. To show history
    if (lower == "show history") {
        auto history = StorageHelper::getHistory();
        if (history.empty()) return "No history found.";
        
        std::ostringstream oss;
        oss << "Command History:";
        for (size_t i = 0; i < history.size(); ++i) {
            oss << "\n" << (i+1) << ". " << history[i].first << " -> " << history[i].second;
        }
        return oss.str();
    }

    // 12. To delete history
    if (lower == "clear history" || lower == "delete history") {
        StorageHelper::clearHistory();
        return "History cleared.";
    }


    // 13. Fallback
    return askLlama(command);
}


std::string CommandProcessor::handleFolderOpen(const std::string& command) {
    std::smatch match;
    std::regex pattern(R"(open (.+) folder)");
    if (std::regex_search(command, match, pattern)) {
        std::string folderName = match[1];
        std::string path;

        // Add mappings as needed
        if (folderName == "downloads") path = "~/Downloads";
        else if (folderName == "documents") path = "~/Documents";
        else if (folderName == "desktop") path = "~/Desktop";
        else if (folderName == "pictures") path = "~/Pictures";
        else return "Unknown folder: " + folderName;

#ifdef __APPLE__
        std::string cmd = "open " + path;
#else
        std::string cmd = "xdg-open " + path;
#endif
        system(cmd.c_str());
        return "Opening " + folderName + " folder.";
    }

    return "";
}

std::string CommandProcessor::handleSystemControl(const std::string& command) {
    std::string prefix1 = "open app ";
    std::string prefix2 = "open ";
    std::string lower = command;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    if (lower.rfind(prefix1, 0) == 0) {
        std::string app = command.substr(prefix1.length());
#ifdef __APPLE__
        std::string cmd = "open -a '" + app + "'";
#else
        std::string cmd = app + " &";
#endif
        system(cmd.c_str());
        return "Opening app: " + app;
    } else if (lower.rfind(prefix2, 0) == 0) {
        // Prevent it from trying to open site as app
        if (lower.find(".com") != std::string::npos || lower.find("site") != std::string::npos)
            return "";

        std::string app = command.substr(prefix2.length());
#ifdef __APPLE__
        std::string cmd = "open -a '" + app + "'";
#else
        std::string cmd = app + " &";
#endif
        system(cmd.c_str());
        return "Opening app: " + app;
    }
    return "";
}


std::string CommandProcessor::handleVolumeControl(const std::string& command) {
    std::string lower = command;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

#ifdef __APPLE__
    if (lower.find("set volume to ") == 0) {
        std::string numStr = command.substr(14);
        int volume = std::stoi(numStr);
        volume = std::max(0, std::min(100, volume));
        std::string cmd = "osascript -e 'set volume output volume " + std::to_string(volume) + "'";
        system(cmd.c_str());
        return "Volume set to " + std::to_string(volume) + "%.";
    }
    else if (lower == "mute") {
        system("osascript -e 'set volume with output muted'");
        return "Volume muted.";
    }
    else if (lower == "unmute") {
        system("osascript -e 'set volume without output muted'");
        return "Volume unmuted.";
    }
    else if (lower == "increase volume") {
        system("osascript -e 'set volume output volume ((output volume of (get volume settings)) + 10)'");
        return "Increased volume.";
    }
    else if (lower == "decrease volume") {
        system("osascript -e 'set volume output volume ((output volume of (get volume settings)) - 10)'");
        return "Decreased volume.";
    }
#endif

    return "";
}


std::string CommandProcessor::handleReminders(const std::string& command) {
    if (command.rfind("remind me to ", 0) == 0) {
        std::string reminder = command.substr(13);
        StorageHelper::saveReminder(reminder);
        return "Reminder added: " + reminder;
    } else if (command == "list reminders") {
        auto all = StorageHelper::loadReminders();
        if (all.empty()) return "No reminders.";
        std::ostringstream oss;
        oss << "Reminders:";
        for (size_t i = 0; i < all.size(); ++i) {
            oss << "\n" << (i+1) << ". " << all[i];
        }
        return oss.str();
    }
    return "";
}


std::string CommandProcessor::handleFileSearch(const std::string& command) {
    std::smatch match;
    std::regex pattern(R"(find file (.+))");
    if (std::regex_search(command, match, pattern)) {
        std::string filename = match[1];

#ifdef __APPLE__
        std::string cmd = "mdfind -name \"" + filename + "\"";
#else
        std::string cmd = "find ~ -name \"" + filename + "\" 2>/dev/null | head -n 1";
#endif

        std::array<char, 256> buffer;
        std::string result;
        FILE* pipe = popen(cmd.c_str(), "r");
        if (!pipe) return "Failed to run file search.";

        while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
            result += buffer.data();
        }
        pclose(pipe);

        if (result.empty()) {
            return "File not found.";
        } else {
            return "Found files:\n" + result;
        }

    }

    return "";
}