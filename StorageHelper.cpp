#include "StorageHelper.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <filesystem>

// StorageHelper.cpp
static const std::string DB_FILE = "/path/to/Orion/orion.db";
static const std::string CONFIG_FILE = "config.json";

void StorageHelper::initDatabase() {
    sqlite3* db;
    sqlite3_open(DB_FILE.c_str(), &db);

    const char* sql = "CREATE TABLE IF NOT EXISTS reminders (id INTEGER PRIMARY KEY AUTOINCREMENT, text TEXT);";
    sqlite3_exec(db, sql, nullptr, nullptr, nullptr);

    sqlite3_close(db);
}

void StorageHelper::saveReminder(const std::string& reminder) {
    sqlite3* db;
    sqlite3_open(DB_FILE.c_str(), &db);

    std::string sql = "INSERT INTO reminders (text) VALUES (?);";
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, reminder.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    sqlite3_close(db);
}

std::vector<std::string> StorageHelper::loadReminders() {
    std::vector<std::string> results;
    sqlite3* db;
    sqlite3_open(DB_FILE.c_str(), &db);

    std::string sql = "SELECT text FROM reminders;";
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const unsigned char* text = sqlite3_column_text(stmt, 0);
        if (text) results.push_back(reinterpret_cast<const char*>(text));
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return results;
}

void StorageHelper::saveConfig(const std::string& key, const std::string& value) {
    nlohmann::json j;

    std::ifstream in(CONFIG_FILE);
    if (in) in >> j;

    j[key] = value;
    std::ofstream out(CONFIG_FILE);
    out << j.dump(4);
}

std::string StorageHelper::loadConfig(const std::string& key) {
    nlohmann::json j;
    std::ifstream in(CONFIG_FILE);
    if (in) in >> j;

    if (j.contains(key)) return j[key];
    return "";
}

void logHistory(const std::string& command, const std::string& response);

void StorageHelper::logHistory(const std::string& command, const std::string& response) {
    sqlite3* db;
    sqlite3_open(DB_FILE.c_str(), &db);

    // Create table if not exists
    const char* createSql = "CREATE TABLE IF NOT EXISTS history ("
                            "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                            "command TEXT, "
                            "response TEXT, "
                            "timestamp DATETIME DEFAULT CURRENT_TIMESTAMP);";
    sqlite3_exec(db, createSql, nullptr, nullptr, nullptr);

    // Insert history
    const char* insertSql = "INSERT INTO history (command, response) VALUES (?, ?);";
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db, insertSql, -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, command.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, response.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    sqlite3_close(db);
}

void StorageHelper::initialize() {
    // Initialize the SQLite database
    initDatabase();

    // Create default config.json if it doesn't exist
    std::ifstream in("config.json");
    if (!in.good()) {
        nlohmann::json j;
        j["created_at"] = time(nullptr);
        std::ofstream out("config.json");
        out << j.dump(4);
    }
}

std::vector<std::pair<std::string, std::string>> StorageHelper::getHistory() {
    sqlite3* db;
    std::vector<std::pair<std::string, std::string>> history;
    if (sqlite3_open(DB_FILE.c_str(), &db) != SQLITE_OK) return history;

    std::string sql = "SELECT command, response FROM history ORDER BY id DESC;";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            std::string cmd = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            std::string resp = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            history.emplace_back(cmd, resp);
        }
        sqlite3_finalize(stmt);
    }

    sqlite3_close(db);
    return history;
}

void StorageHelper::clearHistory() {
    sqlite3* db;
    if (sqlite3_open(DB_FILE.c_str(), &db) == SQLITE_OK) {
        std::string sql = "DELETE FROM history;";
        sqlite3_exec(db, sql.c_str(), nullptr, nullptr, nullptr);
        sqlite3_close(db);
    }
}
