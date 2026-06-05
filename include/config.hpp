#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <memory>
#include <iomanip>
#include <sstream>

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
        std::string configPath;
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
        bool ee895Enabled;
        int ee895I2CBus;
        int ee895I2CAddress;
        std::string ee895SensorId;
        bool sds011Enabled;
        std::vector<std::string> sds011Devices;
        bool ina219Enabled;
        int ina219I2CBus;
        int ina219I2CAddress;
        std::string ina219SensorId;
        float ina219ShuntResistance;
        bool vedirectEnabled;
        std::vector<std::string> vedirectDevices;
        std::vector<std::string> vedirectSensorIds;
        std::unordered_map<std::string, std::string> gpioPins;
    };

    // Load service configuration
    ServiceConfig loadServiceConfig();

    // Write service configuration (clean, no comments)
    bool writeServiceConfig(const ServiceConfig& config);
}
