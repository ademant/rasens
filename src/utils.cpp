#include "utils.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <cstring>
#include <algorithm>

std::string rpi::getPlatformInfo() {
    #ifdef __ARM_ARCH
        return "ARM architecture (Raspberry Pi)";
    #else
        return "Unknown architecture";
    #endif
}

std::string rpi::getHostname() {
    char hostname[256];
    if (gethostname(hostname, sizeof(hostname)) == 0) {
        return std::string(hostname);
    }
    return "unknown";
}

void rpi::printSystemInfo() {
    std::cout << "=== Raspberry Pi System Info ===" << std::endl;
    std::cout << "Hostname: " << getHostname() << std::endl;
    std::cout << "Platform: " << getPlatformInfo() << std::endl;
    
    // Read CPU info
    std::ifstream cpuinfo("/proc/cpuinfo");
    std::string line;
    while (std::getline(cpuinfo, line)) {
        if (line.find("model name") != std::string::npos ||
            line.find("Processor") != std::string::npos ||
            line.find("Hardware") != std::string::npos) {
            std::cout << "CPU: " << line << std::endl;
        }
    }
    
    // Read memory info
    std::ifstream meminfo("/proc/meminfo");
    std::getline(meminfo, line);
    std::cout << "Memory: " << line << std::endl;
    
    std::cout << "=================================" << std::endl;
}

// Check if a directory exists
bool directoryExists(const std::string& path) {
    struct stat info;
    return stat(path.c_str(), &info) == 0 && S_ISDIR(info.st_mode);
}

// Find all DS18B20 devices in the 1-wire bus
std::vector<std::string> rpi::findDS18B20Devices() {
    std::vector<std::string> devices;
    const std::string w1DevicesPath = "/sys/bus/w1/devices/";
    
    if (!directoryExists(w1DevicesPath)) {
        return devices; // 1-wire bus not available
    }
    
    DIR* dir = opendir(w1DevicesPath.c_str());
    if (!dir) {
        return devices;
    }
    
    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        std::string name = entry->d_name;
        // DS18B20 sensors typically start with "28-" or "10-"
        if (name.find("28-") == 0 || name.find("10-") == 0) {
            devices.push_back(w1DevicesPath + name);
        }
    }
    
    closedir(dir);
    return devices;
}

// Read temperature from a DS18B20 device
// Returns temperature in Celsius, or -999.0 on error
double rpi::readDSTemperature(const std::string& devicePath) {
    std::string slavePath = devicePath + "/w1_slave";
    std::ifstream file(slavePath);
    
    if (!file.is_open()) {
        return -999.0;
    }
    
    std::string line;
    std::string crcLine;
    std::string tempLine;
    
    // Read the two lines from w1_slave
    if (std::getline(file, crcLine)) {
        if (std::getline(file, tempLine)) {
            // Check CRC
            if (crcLine.find("YES") != std::string::npos) {
                // Find the temperature value: t=12345
                size_t tempPos = tempLine.find("t=");
                if (tempPos != std::string::npos) {
                    std::string tempStr = tempLine.substr(tempPos + 2);
                    try {
                        int rawTemp = std::stoi(tempStr);
                        // Temperature is in millidegrees Celsius
                        return static_cast<double>(rawTemp) / 1000.0;
                    } catch (...) {
                        return -999.0;
                    }
                }
            }
        }
    }
    
    return -999.0;
}

// Read all DS18B20 temperature sensors and return readings
std::vector<rpi::SensorReading> rpi::readAllDSTemperatureSensors() {
    std::vector<SensorReading> readings;
    std::vector<std::string> devices = findDS18B20Devices();
    
    for (const auto& devicePath : devices) {
        // Extract sensor ID from path (e.g., "28-00000c3b4baa")
        size_t lastSlash = devicePath.find_last_of('/');
        std::string sensorId = (lastSlash != std::string::npos) ? 
                               devicePath.substr(lastSlash + 1) : devicePath;
        
        // Read temperature
        double temperature = readDSTemperature(devicePath);
        
        // Only add valid readings (not error value)
        if (temperature > -900.0) {
            readings.push_back({
                "ds18",           // sensor_type
                sensorId,         // sensor_id
                "temperature",    // measurement
                temperature       // value in Celsius
            });
        }
    }
    
    return readings;
}

bool rpi::setupGPIO() {
    // Implementation depends on library
    // wiringPi: return wiringPiSetup() == 0;
    // pigpio: return gpiod_chip_open("/dev/gpiochip0") != nullptr;
    std::cout << "GPIO setup placeholder - implement with your preferred library" << std::endl;
    return false;
}

bool rpi::setPinMode(int pin, PinMode mode) {
    // Implementation placeholder
    return false;
}

bool rpi::digitalWrite(int pin, bool value) {
    // Implementation placeholder
    return false;
}

bool rpi::digitalRead(int pin) {
    // Implementation placeholder
    return false;
}
