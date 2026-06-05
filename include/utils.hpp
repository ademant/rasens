#pragma once

#include <string>
#include <vector>
#include <map>
#include <thread>
#include <mutex>
#include <atomic>

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

    // ==================== SDS011 Dust Sensor ====================
    
    // SDS011 sensor reading structure
    struct SDS011Reading {
        double pm2_5;
        double pm10;
        bool valid;
    };

    // Serial device info structure
    struct SerialDeviceInfo {
        std::string devicePath;
        std::string name;
        std::string manufacturer;
        std::string serialNumber;
    };

    // Scan for available serial (USB) devices
    std::vector<SerialDeviceInfo> scanSerialDevices();

    // Detect SDS011 sensor by trying to read from serial devices
    std::string detectSDS011Device();

    // Check if a device at given path is an SDS011 sensor
    bool isSDS011AtDevice(const std::string& devicePath);

    // Read from SDS011 sensor
    SDS011Reading readSDS011(const std::string& devicePath);

    // Read from SDS011 sensor at specified device path
    std::vector<SensorReading> readSDS011Sensor(const std::string& devicePath, const std::string& sensorId);

    // ==================== INA219 Current/Voltage/Power Monitor ====================

    static constexpr int INA219_I2C_ADDRESS_0 = 0x40;
    static constexpr int INA219_I2C_ADDRESS_1 = 0x41;
    static constexpr int INA219_I2C_ADDRESS_2 = 0x44;
    static constexpr int INA219_I2C_ADDRESS_3 = 0x45;

    bool readINA219(int fd, double shuntResistance,
                    double& busVoltage, double& shuntVoltage,
                    double& current, double& power);

    std::vector<SensorReading> readINA219Sensor(int i2cBus, int address,
                                                 const std::string& sensorId,
                                                 double shuntResistance);

    // ==================== VE.Direct Serial Telemetry ====================

    class VEDirectReader {
    public:
        VEDirectReader(const std::string& devicePath, const std::string& sensorId);
        ~VEDirectReader();
        VEDirectReader(const VEDirectReader&) = delete;
        VEDirectReader& operator=(const VEDirectReader&) = delete;

        std::vector<SensorReading> getReadings() const;
        bool waitForReading(int timeoutMs) const;

    private:
        void threadFunc();
        static std::vector<SensorReading> parseFrame(
            const std::string& sensorId,
            const std::map<std::string, std::string>& kv);

        std::string devicePath_;
        std::string sensorId_;
        mutable std::mutex mutex_;
        std::vector<SensorReading> snapshot_;
        std::atomic<bool> stop_{false};
        std::thread thread_;
    };

    bool isVEDirectDevice(const std::string& devicePath);
}
