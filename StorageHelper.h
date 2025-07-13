#pragma once
#include <string>
#include <vector>
#include <sqlite3.h>
#include <nlohmann/json.hpp>

class StorageHelper {
public:
    static void initDatabase();
    static void initialize(); // ⬅️ Add this declaration
    static void saveReminder(const std::string& reminder);
    static std::vector<std::string> loadReminders();

    static void saveConfig(const std::string& key, const std::string& value);
    static std::string loadConfig(const std::string& key);

    static void logHistory(const std::string& userCommand, const std::string& response);
    
    static std::vector<std::pair<std::string, std::string>> getHistory();

    static void clearHistory();

};
