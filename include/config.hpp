#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <memory>

namespace rpi {
    class ConfigParser {
    public:
        ConfigParser();
        ~ConfigParser();
        
        // Load configuration from file
        bool load(const std::string& filepath);
        
        // Get value as string
        std::string getString(const std::string& section, const std::string& key, const std::string& defaultValue = "") const;
        
        // Get value as integer
        int getInt(const std::string& section, const std::string& key, int defaultValue = 0) const;
        
        // Get value as boolean
        bool getBool(const std::string& section, const std::string& key, bool defaultValue = false) const;
        
        // Get all keys in a section
        std::vector<std::string> getKeys(const std::string& section) const;
        
        // Check if section exists
        bool hasSection(const std::string& section) const;
        
        // Check if key exists in section
        bool hasKey(const std::string& section, const std::string& key) const;
        
        // Default config path
        static const std::string DEFAULT_CONFIG_PATH;
        
    private:
        struct Impl;
        std::unique_ptr<Impl> impl;
    };
    
    // Global configuration singleton
    ConfigParser& getConfig();
    
    // Service configuration structure
    struct ServiceConfig {
        std::string name;
        std::string logLevel;
        std::string logFile;
        std::string pidFile;
        bool networkEnabled;
        int port;
        std::string bindAddress;
        bool gpioEnabled;
        int pollIntervalMs;
        bool sensorLogging;
        bool ds18b20Enabled;
        std::unordered_map<std::string, std::string> gpioPins;
    };
    
    // Load service configuration
    ServiceConfig loadServiceConfig();
}
