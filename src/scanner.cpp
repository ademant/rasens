#include "scanner.hpp"
#include "utils.hpp"
#include <iostream>
#include <iomanip>
#include <unistd.h>

namespace rpi {

static bool ee895RespondsAt(int bus, int address) {
    int fd = openI2CDevice(bus, address);
    if (fd < 0) return false;
    double co2, temp, pressure;
    bool ok = readEE895(fd, co2, temp, pressure);
    close(fd);
    return ok;
}

void scanAndUpdateConfig(ServiceConfig& config) {
    // EE895
    if (config.ee895Enabled) {
        if (config.ee895I2CAddress != 0 && ee895RespondsAt(config.ee895I2CBus, config.ee895I2CAddress)) {
            std::cout << "[SCAN] EE895: already configured at 0x"
                      << std::hex << config.ee895I2CAddress << std::dec
                      << " on bus " << config.ee895I2CBus << ", skipping\n";
        } else {
            std::cout << "[SCAN] EE895: probing I2C bus " << config.ee895I2CBus << "...\n";
            bool found = false;
            for (int addr : {EE895_I2C_ADDRESS_DEFAULT, EE895_I2C_ADDRESS_ALT}) {
                if (ee895RespondsAt(config.ee895I2CBus, addr)) {
                    config.ee895I2CAddress = addr;
                    std::cout << "[SCAN] EE895: found at 0x" << std::hex << addr << std::dec << "\n";
                    found = true;
                    break;
                }
            }
            if (!found) {
                std::cerr << "[SCAN] WARNING: EE895 enabled but not found on I2C bus "
                          << config.ee895I2CBus << " - check wiring\n";
            }
        }
    }

    // SDS011
    if (config.sds011Enabled) {
        // Verify which configured devices still respond
        std::vector<std::string> verified;
        for (const auto& dev : config.sds011Devices) {
            if (isSDS011AtDevice(dev)) {
                verified.push_back(dev);
            } else {
                std::cout << "[SCAN] SDS011: configured device " << dev
                          << " not responding, will re-scan\n";
            }
        }

        if (!verified.empty() && verified.size() == config.sds011Devices.size()) {
            std::cout << "[SCAN] SDS011: " << verified.size()
                      << " device(s) already configured and responding, skipping\n";
            return;
        }

        // Scan for new devices, starting from any already-verified ones
        std::cout << "[SCAN] SDS011: scanning serial devices...\n";
        std::vector<std::string> found = verified;

        for (const auto& info : scanSerialDevices()) {
            bool alreadyKnown = false;
            for (const auto& f : found) {
                if (f == info.devicePath) { alreadyKnown = true; break; }
            }
            if (alreadyKnown) continue;

            std::cout << "[SCAN] SDS011: probing " << info.devicePath << "...\n";
            if (isSDS011AtDevice(info.devicePath)) {
                found.push_back(info.devicePath);
                std::cout << "[SCAN] SDS011: found at " << info.devicePath << "\n";
            }
        }

        if (found.empty()) {
            std::cerr << "[SCAN] WARNING: SDS011 enabled but no device found - check USB connection\n";
        } else {
            config.sds011Devices = found;
        }
    }
}

} // namespace rpi
