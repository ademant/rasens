#pragma once

#include <string>
#include <vector>
#include <map>

namespace rpi {
    // Simple utility functions for Raspberry Pi projects
    
    std::string getPlatformInfo();
    void printSystemInfo();
    std::string getHostname();
    
    // Raspberry Pi GPIO pin modes
    enum class PinMode {
        INPUT,
        OUTPUT,
        PWM_OUTPUT,
        GPIO_CLOCK
    };
    
    // Placeholder for GPIO functions
    // Implement these using wiringPi, pigpio, or sysfs
    bool setupGPIO();
    bool setPinMode(int pin, PinMode mode);
    bool digitalWrite(int pin, bool value);
    bool digitalRead(int pin);
    
    // Sensor data structure
    struct SensorReading {
        std::string sensor_type;
        std::string sensor_id;
        std::string measurement;
        double value;
    };
    
    // DS18B20 1-Wire temperature sensor functions
    std::vector<SensorReading> readAllDSTemperatureSensors();
    double readDSTemperature(const std::string& devicePath);
    std::vector<std::string> findDS18B20Devices();
}
