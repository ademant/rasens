#include "config.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <iostream>
#include <iomanip>

namespace rpi {
    const std::string ConfigParser::DEFAULT_CONFIG_PATH = "/etc/rasens/config.conf";
    
    struct ConfigParser::Impl {
        std::unordered_map<std::string, std::unordered_map<std::string, std::string>> sections;
        
        static std::string trim(const std::string& str) {
            auto start = str.begin();
            while (start != str.end() && std::isspace(*start)) start++;
            auto end = str.end();
            while (end != start && std::isspace(*(end - 1))) end--;
            return std::string(start, end);
        }
        
        static std::string toLower(const std::string& str) {
            std::string result = str;
            std::transform(result.begin(), result.end(), result.begin(), 
                [](unsigned char c) { return std::tolower(c); });
            return result;
        }
    };
    
    ConfigParser::ConfigParser() : impl(std::make_unique<Impl>()) {}
    ConfigParser::~ConfigParser() = default;
    
    bool ConfigParser::load(const std::string& filepath) {
        std::ifstream file(filepath);
        if (!file.is_open()) {
            return false;
        }
        
        std::string line;
        std::string currentSection;
        
        while (std::getline(file, line)) {
            line = Impl::trim(line);
            
            // Skip empty lines and comments
            if (line.empty() || line[0] == '#' || line[0] == ';') {
                continue;
            }
            
            // Section header
            if (line[0] == '[' && line.back() == ']') {
                currentSection = Impl::trim(line.substr(1, line.size() - 2));
                continue;
            }
            
            // Key-value pair
            size_t delimiterPos = line.find('=');
            if (delimiterPos != std::string::npos) {
                std::string key = Impl::trim(line.substr(0, delimiterPos));
                std::string value = Impl::trim(line.substr(delimiterPos + 1));
                
                if (!currentSection.empty()) {
                    impl->sections[currentSection][key] = value;
                }
            }
        }
        
        return true;
    }
    
    std::string ConfigParser::getString(const std::string& section, const std::string& key, const std::string& defaultValue) const {
        auto sectionIt = impl->sections.find(section);
        if (sectionIt != impl->sections.end()) {
            auto keyIt = sectionIt->second.find(key);
            if (keyIt != sectionIt->second.end()) {
                return keyIt->second;
            }
        }
        return defaultValue;
    }
    
    int ConfigParser::getInt(const std::string& section, const std::string& key, int defaultValue) const {
        std::string value = getString(section, key, "");
        if (!value.empty()) {
            try {
                return std::stoi(value, nullptr, 0);
            } catch (...) {
                return defaultValue;
            }
        }
        return defaultValue;
    }
    
    bool ConfigParser::getBool(const std::string& section, const std::string& key, bool defaultValue) const {
        std::string value = getString(section, key, "");
        if (!value.empty()) {
            std::string lower = Impl::toLower(value);
            if (lower == "true" || lower == "yes" || lower == "1" || lower == "on") {
                return true;
            }
            if (lower == "false" || lower == "no" || lower == "0" || lower == "off") {
                return false;
            }
        }
        return defaultValue;
    }
    
    std::vector<std::string> ConfigParser::getKeys(const std::string& section) const {
        std::vector<std::string> keys;
        auto sectionIt = impl->sections.find(section);
        if (sectionIt != impl->sections.end()) {
            for (const auto& kv : sectionIt->second) {
                keys.push_back(kv.first);
            }
        }
        return keys;
    }
    
    bool ConfigParser::hasSection(const std::string& section) const {
        return impl->sections.find(section) != impl->sections.end();
    }
    
    bool ConfigParser::hasKey(const std::string& section, const std::string& key) const {
        auto sectionIt = impl->sections.find(section);
        if (sectionIt != impl->sections.end()) {
            return sectionIt->second.find(key) != sectionIt->second.end();
        }
        return false;
    }
    
    // Global configuration instance
    ConfigParser& getConfig() {
        static ConfigParser instance;
        return instance;
    }
    
    ServiceConfig loadServiceConfig() {
        ServiceConfig config;
        ConfigParser& parser = getConfig();

        const char* envPath = std::getenv("RASENS_CONFIG");
        std::string path = envPath ? envPath : ConfigParser::DEFAULT_CONFIG_PATH;

        if (parser.load(path)) {
            config.configPath = path;
        } else if (parser.load("config/rasens.conf")) {
            config.configPath = "config/rasens.conf";
        } else {
            std::cerr << "Warning: Could not load configuration from " << path << " or config/rasens.conf" << std::endl;
            return config;
        }

        // General section
        config.name = parser.getString("General", "name", "rasens");
        config.logLevel = parser.getString("General", "log_level", "INFO");
        config.logFile = parser.getString("General", "log_file", "/var/log/rasens/service.log");
        config.pidFile = parser.getString("General", "pid_file", "/var/run/rasens.pid");

        // Network section
        config.networkEnabled = parser.getBool("Network", "enabled", false);
        config.port = parser.getInt("Network", "port", 8080);
        config.bindAddress = parser.getString("Network", "bind_address", "0.0.0.0");

        // GPIO section
        config.gpioEnabled = parser.getBool("GPIO", "enabled", true);
        for (const auto& key : parser.getKeys("GPIO")) {
            if (key.find("pin_") == 0)
                config.gpioPins[key] = parser.getString("GPIO", key, "");
        }

        // Sensors section (general)
        config.pollIntervalMs = parser.getInt("Sensors", "poll_interval_ms", 1000);
        config.sensorLogging = parser.getBool("Sensors", "enable_logging", true);

        // DS18B20
        config.ds18b20Enabled = parser.getBool("DS18B20", "enabled", true);

        // EE895 CO2 sensor
        config.ee895Enabled = parser.getBool("EE895", "enabled", false);
        config.ee895I2CBus = parser.getInt("EE895", "i2c_bus", 1);
        config.ee895I2CAddress = parser.getInt("EE895", "i2c_address", 0);
        config.ee895SensorId = parser.getString("EE895", "sensor_id", "ee895-1");

        // SDS011 dust sensor
        config.sds011Enabled = parser.getBool("SDS011", "enabled", false);
        std::string devicesStr = parser.getString("SDS011", "devices", "");
        if (!devicesStr.empty()) {
            std::istringstream ss(devicesStr);
            std::string token;
            while (std::getline(ss, token, ',')) {
                auto s = token.begin();
                while (s != token.end() && std::isspace((unsigned char)*s)) ++s;
                auto e = token.end();
                while (e != s && std::isspace((unsigned char)*(e - 1))) --e;
                std::string trimmed(s, e);
                if (!trimmed.empty())
                    config.sds011Devices.push_back(trimmed);
            }
        }

        return config;
    }

    bool writeServiceConfig(const ServiceConfig& config) {
        if (config.configPath.empty()) {
            std::cerr << "Error: no config path to write to" << std::endl;
            return false;
        }

        std::ofstream f(config.configPath);
        if (!f.is_open()) {
            std::cerr << "Error: cannot write config to " << config.configPath << std::endl;
            return false;
        }

        f << "[General]\n"
          << "name = " << config.name << "\n"
          << "log_level = " << config.logLevel << "\n"
          << "log_file = " << config.logFile << "\n"
          << "pid_file = " << config.pidFile << "\n\n";

        f << "[Network]\n"
          << "enabled = " << (config.networkEnabled ? "true" : "false") << "\n"
          << "port = " << config.port << "\n"
          << "bind_address = " << config.bindAddress << "\n\n";

        f << "[GPIO]\n"
          << "enabled = " << (config.gpioEnabled ? "true" : "false") << "\n";
        for (const auto& [key, val] : config.gpioPins)
            f << key << " = " << val << "\n";
        f << "\n";

        f << "[Sensors]\n"
          << "poll_interval_ms = " << config.pollIntervalMs << "\n"
          << "enable_logging = " << (config.sensorLogging ? "true" : "false") << "\n\n";

        f << "[DS18B20]\n"
          << "enabled = " << (config.ds18b20Enabled ? "true" : "false") << "\n\n";

        f << "[EE895]\n"
          << "enabled = " << (config.ee895Enabled ? "true" : "false") << "\n"
          << "i2c_bus = " << config.ee895I2CBus << "\n"
          << "i2c_address = 0x" << std::hex << config.ee895I2CAddress << std::dec << "\n"
          << "sensor_id = " << config.ee895SensorId << "\n\n";

        f << "[SDS011]\n"
          << "enabled = " << (config.sds011Enabled ? "true" : "false") << "\n";
        if (!config.sds011Devices.empty()) {
            f << "devices = ";
            for (size_t i = 0; i < config.sds011Devices.size(); ++i) {
                if (i > 0) f << ",";
                f << config.sds011Devices[i];
            }
            f << "\n";
        }

        return true;
    }
}
