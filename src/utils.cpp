#include "utils.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <cstring>
#include <algorithm>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include <iomanip>
#include <vector>
#include <thread>
#include <chrono>
#include <termios.h>
#include <string.h>
#include <glob.h>
#include <unordered_map>

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

float rpi::readDSTemperature(const std::string& devicePath) {
    std::string slavePath = devicePath + "/w1_slave";
    std::ifstream file(slavePath);
    if (!file.is_open()) return -999.0f;

    std::string crcLine, tempLine;
    if (std::getline(file, crcLine) && std::getline(file, tempLine)) {
        if (crcLine.find("YES") != std::string::npos) {
            size_t tempPos = tempLine.find("t=");
            if (tempPos != std::string::npos) {
                try {
                    return std::stoi(tempLine.substr(tempPos + 2)) / 1000.0f;
                } catch (...) {}
            }
        }
    }
    return -999.0f;
}

std::vector<rpi::SensorReading> rpi::readAllDSTemperatureSensors() {
    std::vector<SensorReading> readings;
    for (const auto& devicePath : findDS18B20Devices()) {
        size_t lastSlash = devicePath.find_last_of('/');
        std::string sensorId = (lastSlash != std::string::npos)
            ? devicePath.substr(lastSlash + 1) : devicePath;
        float temperature = readDSTemperature(devicePath);
        if (temperature > -900.0f)
            readings.push_back({"ds18", sensorId, "temperature", temperature});
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

// Open I2C device at specified bus and address
int rpi::openI2CDevice(int i2cBus, int address) {
    std::string i2cDevice = "/dev/i2c-" + std::to_string(i2cBus);
    int fd = open(i2cDevice.c_str(), O_RDWR);
    if (fd < 0) {
        return -1;
    }
    if (ioctl(fd, I2C_SLAVE, address) < 0) {
        close(fd);
        return -1;
    }
    return fd;
}

static void closeI2CDevice(int fd) {
    if (fd >= 0) {
        close(fd);
    }
}

static bool i2cReadBytes(int fd, uint8_t* buffer, int length) {
    if (read(fd, buffer, length) != length) {
        return false;
    }
    return true;
}

static bool i2cWriteBytes(int fd, const uint8_t* buffer, int length) {
    if (write(fd, buffer, length) != length) {
        return false;
    }
    return true;
}

// Read EE895 sensor data
// The EE895 provides CO2 (ppm), temperature (C), and pressure (hPa)
// Communication protocol: Send 0x00 command, then read 9 bytes of data
bool rpi::readEE895(int fd, float& co2, float& temperature, float& pressure) {
    uint8_t cmd = 0x00;
    uint8_t data[9] = {0};

    if (!i2cWriteBytes(fd, &cmd, 1)) return false;
    usleep(50000);
    if (!i2cReadBytes(fd, data, 9)) return false;

    co2         = static_cast<float>((data[0] << 8) | data[1]);
    temperature = static_cast<int16_t>((data[2] << 8) | data[3]) / 100.0f;
    pressure    = static_cast<float>((data[4] << 8) | data[5]) / 10.0f;
    return true;
}

std::vector<rpi::SensorReading> rpi::readEE895Sensor(int i2cBus, int address, const std::string& sensorId) {
    std::vector<SensorReading> readings;
    int fd = openI2CDevice(i2cBus, address);
    if (fd < 0) return readings;

    float co2, temperature, pressure;
    if (readEE895(fd, co2, temperature, pressure)) {
        readings.push_back({"ee895", sensorId, "co2",         co2});
        readings.push_back({"ee895", sensorId, "temperature", temperature});
        readings.push_back({"ee895", sensorId, "pressure",    pressure});
    }

    closeI2CDevice(fd);
    return readings;
}

// Read from EE895 sensor - tries default address (0x5A) first, then alt (0x5B)
std::vector<rpi::SensorReading> rpi::readEE895Sensor(int i2cBus, const std::string& sensorId) {
    // Try default address first
    auto readings = readEE895Sensor(i2cBus, EE895_I2C_ADDRESS_DEFAULT, sensorId);
    if (!readings.empty()) {
        return readings;
    }
    // Try alternate address
    return readEE895Sensor(i2cBus, EE895_I2C_ADDRESS_ALT, sensorId);
}

// ============================================================================
// SDS011 Dust Sensor Implementation
// Based on the working C program at /home/ademant/src/rasens/sds011/sds011.c
// ============================================================================

rpi::SDS011Reading rpi::readSDS011(const std::string& devicePath) {
    SDS011Reading reading = {0.0, 0.0, false};
    
    int fd = open(devicePath.c_str(), O_RDWR | O_NOCTTY | O_SYNC);
    if (fd < 0) {
        // Try without O_SYNC
        fd = open(devicePath.c_str(), O_RDWR | O_NOCTTY);
        if (fd < 0) {
            return reading;
        }
    }
    
    // Configure serial port - exactly as the working C program does
    struct termios tty;
    memset(&tty, 0, sizeof tty);
    if (tcgetattr(fd, &tty) != 0) {
        close(fd);
        return reading;
    }
    
    cfsetospeed(&tty, B9600);
    cfsetispeed(&tty, B9600);
    
    tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;
    tty.c_iflag &= ~IGNBRK;
    tty.c_lflag = 0;
    tty.c_oflag = 0;
    tty.c_cc[VMIN] = 0;
    tty.c_cc[VTIME] = 5;
    tty.c_iflag &= ~(IXON | IXOFF | IXANY);
    tty.c_cflag |= (CLOCAL | CREAD);
    tty.c_cflag &= ~(PARENB | PARODD);
    tty.c_cflag &= ~CSTOPB;
    tty.c_cflag &= ~CRTSCTS;
    
    if (tcsetattr(fd, TCSANOW, &tty) != 0) {
        close(fd);
        return reading;
    }
    
    // Flush any stale data
    tcflush(fd, TCIFLUSH);

    // Accumulate bytes and scan for a valid framed packet (start=0xAA, tail=0xAB)
    uint8_t buf[10];
    int collected = 0;
    for (int attempt = 0; attempt < 40 && !reading.valid; attempt++) {
        ssize_t n = read(fd, buf + collected, sizeof(buf) - collected);
        if (n > 0) collected += n;

        // Slide through collected bytes looking for a complete valid packet
        while (collected >= 10) {
            if (buf[0] == 0xAA && buf[1] == 0xC0 && buf[9] == 0xAB) {
                uint8_t checksum = 0;
                for (int i = 2; i <= 7; i++) checksum += buf[i];
                if (checksum == buf[8]) {
                    reading.pm2_5    = ((buf[3] << 8) | buf[2]) / 10.0f;
                    reading.pm10     = ((buf[5] << 8) | buf[4]) / 10.0f;
                    reading.deviceId = static_cast<uint16_t>((buf[7] << 8) | buf[6]);
                    reading.valid    = true;
                    break;
                }
            }
            // Discard first byte and try to re-sync
            memmove(buf, buf + 1, --collected);
        }
        if (!reading.valid) usleep(50000);
    }

    close(fd);
    return reading;
}

bool rpi::isSDS011AtDevice(const std::string& devicePath) {
    SDS011Reading reading = readSDS011(devicePath);
    return reading.valid;
}

std::vector<rpi::SerialDeviceInfo> rpi::scanSerialDevices() {
    std::vector<SerialDeviceInfo> devices;
    
    std::vector<std::string> patterns = {
        "/dev/ttyUSB*",
        "/dev/ttyACM*",
        "/dev/ttyS*",
        "/dev/serial/by-id/*",
        "/dev/serial/by-path/*"
    };
    
    for (const auto& pattern : patterns) {
        glob_t globResult;
        if (glob(pattern.c_str(), GLOB_NOSORT, nullptr, &globResult) == 0) {
            for (size_t i = 0; i < globResult.gl_pathc; i++) {
                std::string devicePath = globResult.gl_pathv[i];
                
                bool alreadyAdded = false;
                for (const auto& existing : devices) {
                    if (existing.devicePath == devicePath) {
                        alreadyAdded = true;
                        break;
                    }
                }
                
                if (!alreadyAdded) {
                    SerialDeviceInfo info;
                    info.devicePath = devicePath;
                    info.name = "Unknown";
                    info.manufacturer = "";
                    info.serialNumber = "";
                    
                    if (devicePath.find("/dev/serial/by-id/") != std::string::npos) {
                        info.name = "USB Serial by ID";
                    } else if (devicePath.find("/dev/serial/by-path/") != std::string::npos) {
                        info.name = "USB Serial by Path";
                    } else if (devicePath.find("ttyUSB") != std::string::npos) {
                        info.name = "USB Serial Converter";
                    } else if (devicePath.find("ttyACM") != std::string::npos) {
                        info.name = "CDC-ACM Device";
                    } else if (devicePath.find("ttyS") != std::string::npos) {
                        info.name = "Built-in Serial";
                    }
                    
                    devices.push_back(info);
                }
            }
            globfree(&globResult);
        }
    }
    
    return devices;
}

std::string rpi::detectSDS011Device() {
    std::vector<SerialDeviceInfo> devices = scanSerialDevices();
    for (const auto& device : devices) {
        if (isSDS011AtDevice(device.devicePath)) {
            return device.devicePath;
        }
    }
    return "";
}

std::vector<rpi::SensorReading> rpi::readSDS011Sensor(const std::string& devicePath) {
    std::vector<SensorReading> readings;
    SDS011Reading rawReading = readSDS011(devicePath);

    if (rawReading.valid) {
        char idBuf[5];
        std::snprintf(idBuf, sizeof(idBuf), "%04x", rawReading.deviceId);
        std::string id(idBuf);
        readings.push_back({"sds011", id, "pm2_5",     rawReading.pm2_5});
        readings.push_back({"sds011", id, "pm10",      rawReading.pm10});
        readings.push_back({"sds011", id, "device_id", static_cast<float>(rawReading.deviceId)});
    }

    return readings;
}

// ============================================================================
// INA219 Current/Voltage/Power Monitor
// ============================================================================

bool rpi::readINA219(int fd, float shuntResistance,
                     float& busVoltage, float& shuntVoltage,
                     float& current, float& power)
{
    const float currentLSB = 0.0001f;
    uint16_t cal = static_cast<uint16_t>(0.04096f / (currentLSB * shuntResistance));
    uint8_t calBuf[3] = {0x05, static_cast<uint8_t>(cal >> 8), static_cast<uint8_t>(cal & 0xFF)};
    if (write(fd, calBuf, 3) != 3) return false;

    usleep(2000);

    uint8_t reg, buf[2];

    reg = 0x01;
    if (write(fd, &reg, 1) != 1) return false;
    if (read(fd, buf, 2) != 2) return false;
    shuntVoltage = static_cast<int16_t>((buf[0] << 8) | buf[1]) * 0.00001f;

    reg = 0x02;
    if (write(fd, &reg, 1) != 1) return false;
    if (read(fd, buf, 2) != 2) return false;
    busVoltage = (static_cast<uint16_t>((buf[0] << 8) | buf[1]) >> 3) * 0.004f;

    reg = 0x04;
    if (write(fd, &reg, 1) != 1) return false;
    if (read(fd, buf, 2) != 2) return false;
    current = static_cast<int16_t>((buf[0] << 8) | buf[1]) * currentLSB;

    reg = 0x03;
    if (write(fd, &reg, 1) != 1) return false;
    if (read(fd, buf, 2) != 2) return false;
    power = static_cast<uint16_t>((buf[0] << 8) | buf[1]) * 20.0f * currentLSB;

    return true;
}

std::vector<rpi::SensorReading> rpi::readINA219Sensor(int i2cBus, int address,
                                                        const std::string& sensorId,
                                                        float shuntResistance)
{
    std::vector<SensorReading> readings;
    if (address == 0) return readings;
    int fd = openI2CDevice(i2cBus, address);
    if (fd < 0) return readings;

    float busVoltage, shuntVoltage, current, power;
    if (readINA219(fd, shuntResistance, busVoltage, shuntVoltage, current, power)) {
        readings.push_back({"ina219", sensorId, "bus_voltage",   busVoltage});
        readings.push_back({"ina219", sensorId, "shunt_voltage", shuntVoltage});
        readings.push_back({"ina219", sensorId, "current",       current});
        readings.push_back({"ina219", sensorId, "power",         power});
    }

    closeI2CDevice(fd);
    return readings;
}

// ============================================================================
// SDS011 Threaded Reader
// ============================================================================

void rpi::SDS011Reader::threadFunc() {
    while (!stop_) {
        int fd = open(devicePath_.c_str(), O_RDWR | O_NOCTTY);
        if (fd < 0) {
            std::this_thread::sleep_for(std::chrono::seconds(5));
            continue;
        }

        struct termios tty;
        memset(&tty, 0, sizeof(tty));
        tcgetattr(fd, &tty);
        cfsetospeed(&tty, B9600);
        cfsetispeed(&tty, B9600);
        tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;
        tty.c_iflag &= ~IGNBRK;
        tty.c_lflag = 0;
        tty.c_oflag = 0;
        tty.c_cc[VMIN] = 0;
        tty.c_cc[VTIME] = 10; // 1 second read timeout to allow stop_ check
        tty.c_iflag &= ~(IXON | IXOFF | IXANY);
        tty.c_cflag |= (CLOCAL | CREAD);
        tty.c_cflag &= ~(PARENB | PARODD | CSTOPB | CRTSCTS);
        tcsetattr(fd, TCSANOW, &tty);
        tcflush(fd, TCIFLUSH);

        uint8_t buf[10];
        int collected = 0;

        while (!stop_) {
            uint8_t c;
            if (read(fd, &c, 1) != 1) continue;

            buf[collected++] = c;
            if (collected < 10) continue;

            if (buf[0] == 0xAA && buf[1] == 0xC0 && buf[9] == 0xAB) {
                uint8_t checksum = 0;
                for (int i = 2; i <= 7; i++) checksum += buf[i];
                if (checksum == buf[8]) {
                    uint16_t devId = static_cast<uint16_t>((buf[7] << 8) | buf[6]);
                    char idBuf[5];
                    std::snprintf(idBuf, sizeof(idBuf), "%04x", devId);
                    std::string id(idBuf);
                    float pm2_5 = ((buf[3] << 8) | buf[2]) / 10.0f;
                    float pm10  = ((buf[5] << 8) | buf[4]) / 10.0f;
                    std::vector<SensorReading> readings = {
                        {"sds011", id, "pm2_5",     pm2_5},
                        {"sds011", id, "pm10",      pm10},
                        {"sds011", id, "device_id", static_cast<float>(devId)},
                    };
                    std::lock_guard<std::mutex> lock(mutex_);
                    snapshot_ = std::move(readings);
                    collected = 0;
                    continue;
                }
            }
            memmove(buf, buf + 1, --collected);
        }

        close(fd);
    }
}

rpi::SDS011Reader::SDS011Reader(const std::string& devicePath)
    : devicePath_(devicePath), thread_([this]{ threadFunc(); })
{}

rpi::SDS011Reader::~SDS011Reader() {
    stop_ = true;
    thread_.join();
}

std::vector<rpi::SensorReading> rpi::SDS011Reader::getReadings() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return snapshot_;
}

// ============================================================================
// VE.Direct Serial Telemetry
// ============================================================================

static void setupVEDirectPort(int fd) {
    struct termios tty;
    memset(&tty, 0, sizeof(tty));
    tcgetattr(fd, &tty);
    cfsetispeed(&tty, B19200);
    tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;
    tty.c_iflag &= ~IGNBRK;
    tty.c_lflag = 0;
    tty.c_oflag = 0;
    tty.c_cc[VMIN] = 0;
    tty.c_cc[VTIME] = 10; // 1 second read timeout
    tty.c_iflag &= ~(IXON | IXOFF | IXANY);
    tty.c_cflag |= (CLOCAL | CREAD);
    tty.c_cflag &= ~(PARENB | PARODD | CSTOPB | CRTSCTS);
    tcsetattr(fd, TCSANOW, &tty);
    tcflush(fd, TCIFLUSH);
}

std::vector<rpi::SensorReading> rpi::VEDirectReader::parseFrame(
    const std::string& sensorId,
    const std::map<std::string, std::string>& kv)
{
    static const std::unordered_map<std::string, std::pair<std::string, float>> known = {
        {"V",    {"battery_voltage",   0.001f}},
        {"VS",   {"starter_voltage",   0.001f}},
        {"VM",   {"mid_voltage",       0.001f}},
        {"VPV",  {"panel_voltage",     0.001f}},
        {"PPV",  {"panel_power",       1.0f}},
        {"I",    {"battery_current",   0.001f}},
        {"IL",   {"load_current",      0.001f}},
        {"T",    {"battery_temp",      1.0f}},
        {"P",    {"instantaneous_power",1.0f}},
        {"CE",   {"consumed_energy",   0.001f}},
        {"SOC",  {"state_of_charge",   0.1f}},
        {"TTG",  {"time_to_go",        1.0f}},
        {"ERR",  {"error_code",        1.0f}},
        {"CS",   {"charge_state",      1.0f}},
        {"MPPT", {"mppt_mode",         1.0f}},
        {"H1",   {"depth_deepest_discharge", 0.001f}},
        {"H2",   {"depth_last_discharge",    0.001f}},
        {"H3",   {"depth_avg_discharge",     0.001f}},
        {"H4",   {"charge_cycles",           1.0f}},
        {"H5",   {"full_discharges",         1.0f}},
        {"H6",   {"cumulative_energy",       0.001f}},
        {"H7",   {"min_voltage",             0.001f}},
        {"H8",   {"max_voltage",             0.001f}},
        {"H9",   {"seconds_since_full",      1.0f}},
        {"H17",  {"yield_total_discharged",  0.01f}},
        {"H18",  {"yield_total_charged",     0.01f}},
        {"H19",  {"yield_total",             0.01f}},
        {"H20",  {"yield_today",             0.01f}},
        {"H21",  {"max_power_today",         1.0f}},
        {"H22",  {"yield_yesterday",         0.01f}},
        {"H23",  {"max_power_yesterday",     1.0f}},
        {"HSDS", {"day_sequence",            1.0f}},
    };

    std::vector<SensorReading> readings;
    for (const auto& [label, rawVal] : kv) {
        auto it = known.find(label);
        std::string measurement = (it != known.end()) ? it->second.first : label;
        float scale = (it != known.end()) ? it->second.second : 1.0f;
        try {
            float raw = std::stof(rawVal);
            readings.push_back({"vedirect", sensorId, measurement, raw * scale});
        } catch (...) {
            // non-numeric value (PID hex string, SER#, etc.) — skip
        }
    }
    return readings;
}

void rpi::VEDirectReader::threadFunc() {
    while (!stop_) {
        int fd = open(devicePath_.c_str(), O_RDONLY | O_NOCTTY);
        if (fd < 0) {
            std::this_thread::sleep_for(std::chrono::seconds(5));
            continue;
        }
        setupVEDirectPort(fd);

        std::map<std::string, std::string> frame;
        uint8_t checksum = 0;
        std::string lineBuf;

        while (!stop_) {
            uint8_t c;
            if (read(fd, &c, 1) != 1) continue; // timeout, check stop_

            checksum += c;

            if (c == '\n') {
                if (!lineBuf.empty() && lineBuf.back() == '\r')
                    lineBuf.pop_back();

                auto tab = lineBuf.find('\t');
                if (tab != std::string::npos) {
                    std::string key = lineBuf.substr(0, tab);
                    std::string val = lineBuf.substr(tab + 1);

                    if (key == "Checksum") {
                        if (checksum == 0 && !frame.empty()) {
                            auto readings = parseFrame(sensorId_, frame);
                            std::lock_guard<std::mutex> lock(mutex_);
                            snapshot_ = std::move(readings);
                        }
                        frame.clear();
                        checksum = 0;
                    } else {
                        frame[key] = val;
                    }
                }
                lineBuf.clear();
            } else {
                lineBuf += static_cast<char>(c);
            }
        }

        close(fd);
    }
}

rpi::VEDirectReader::VEDirectReader(const std::string& devicePath, const std::string& sensorId)
    : devicePath_(devicePath), sensorId_(sensorId), thread_([this]{ threadFunc(); })
{}

rpi::VEDirectReader::~VEDirectReader() {
    stop_ = true;
    thread_.join();
}

std::vector<rpi::SensorReading> rpi::VEDirectReader::getReadings() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return snapshot_;
}

bool rpi::VEDirectReader::waitForReading(int timeoutMs) const {
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    while (std::chrono::steady_clock::now() < deadline) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (!snapshot_.empty()) return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    return false;
}

bool rpi::isVEDirectDevice(const std::string& devicePath) {
    int fd = open(devicePath.c_str(), O_RDONLY | O_NOCTTY);
    if (fd < 0) return false;

    struct termios tty;
    memset(&tty, 0, sizeof(tty));
    tcgetattr(fd, &tty);
    cfsetispeed(&tty, B19200);
    tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;
    tty.c_iflag = 0;
    tty.c_lflag = 0;
    tty.c_oflag = 0;
    tty.c_cc[VMIN] = 0;
    tty.c_cc[VTIME] = 20; // 2 second timeout
    tty.c_cflag |= (CLOCAL | CREAD);
    tty.c_cflag &= ~(PARENB | PARODD | CSTOPB | CRTSCTS);
    tcsetattr(fd, TCSANOW, &tty);
    tcflush(fd, TCIFLUSH);

    // Read a burst and look for CRLF + tab — the VE.Direct signature
    char buf[256];
    ssize_t n = read(fd, buf, sizeof(buf));
    close(fd);

    if (n < 4) return false;
    bool hasCRLF = false, hasTab = false;
    for (ssize_t i = 0; i < n; i++) {
        if (buf[i] == '\t') hasTab = true;
        if (i + 1 < n && buf[i] == '\r' && buf[i+1] == '\n') hasCRLF = true;
    }
    return hasCRLF && hasTab;
}
