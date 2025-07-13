#pragma once
#include <string>
#include <vector>
#include <memory>
#include <map>

class Plugin {
public:
    virtual ~Plugin() = default;
    virtual std::string name() const = 0;
    virtual std::string handle(const std::string& command) = 0;
};

std::string askLlama(const std::string& prompt);

class CommandProcessor {
public:
    CommandProcessor();
    std::string processCommand(const std::string& command);
    void addPlugin(std::shared_ptr<Plugin> plugin);
private:
    std::string handleSystemControl(const std::string& command);
    std::string handleFileSearch(const std::string& command); // ⬅️ Add this line
    std::string handleFolderOpen(const std::string& command);
    std::string handleVolumeControl(const std::string& command);

    std::string handleReminders(const std::string& command);
    std::vector<std::string> reminders;
    std::map<std::string, std::shared_ptr<Plugin>> plugins;
}; 