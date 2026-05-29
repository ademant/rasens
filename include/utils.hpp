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
    
    // EE895 CO2 sensor functions (I2C)
    // Opens I2C device and returns file descriptor
    int openI2CDevice(int i2cBus, int address);
    
    // EE895 sensor reading - returns CO2 in ppm, temperature in C, pressure in hPa
    bool readEE895(int fd, double& co2, double& temperature, double& pressure);
    
    // Read from EE895 sensor at specified I2C bus and address
    std::vector<SensorReading> readEE895Sensor(int i2cBus, int address, const std::string& sensorId);
    
    // Read from EE895 sensor - tries default address (0x5A) first, then alt (0x5B)
    std::vector<SensorReading> readEE895Sensor(int i2cBus, const std::string& sensorId = "default");
    
    // EE895 I2C addresses
    static constexpr int EE895_I2C_ADDRESS_DEFAULT = 0x5A;
    static constexpr int EE895_I2C_ADDRESS_ALT = 0x5B;
}
