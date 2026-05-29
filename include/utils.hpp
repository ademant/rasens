#pragma once

#include <string>
#include <vector>

namespace rpi {
    // Simple utility functions for Raspberry Pi projects
    
    std::string getPlatformInfo();
    void printSystemInfo();
    
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
}
