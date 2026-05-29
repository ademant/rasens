#include <iostream>
#include <string>
#include <fstream>
#include <csignal>
#include <cstdlib>
#include <sys/stat.h>
#include <unistd.h>
#include <iomanip>
#include <vector>

#include "config.hpp"
#include "utils.hpp"

// Global flag for graceful shutdown
static volatile bool running = true;
static std::string globalPidFile;

// Signal handler for graceful shutdown
void signalHandler(int signal) {
    running = false;
}

// Cleanup function for atexit
void cleanupPidFile() {
    if (!globalPidFile.empty()) {
        std::remove(globalPidFile.c_str());
    }
}

// Write PID file
bool writePidFile(const std::string& pidFile) {
    std::ofstream file(pidFile);
    if (!file.is_open()) {
        return false;
    }
    file << getpid();
    return true;
}

// Remove PID file
void removePidFile(const std::string& pidFile) {
    std::remove(pidFile.c_str());
}

// Create directories if they don't exist
void ensureDirectoriesExist(const std::string& path) {
    size_t pos = path.find_last_of('/');
    if (pos != std::string::npos) {
        std::string dir = path.substr(0, pos);
        mkdir(dir.c_str(), 0755);
    }
}

// Simple logging function
void logMessage(const std::string& level, const std::string& message, const std::string& logFile = "") {
    std::string formatted = "[" + level + "] " + message;
    std::cout << formatted << std::endl;
    
    if (!logFile.empty()) {
        ensureDirectoriesExist(logFile);
        std::ofstream file(logFile, std::ios::app);
        if (file.is_open()) {
            file << formatted << std::endl;
        }
    }
}

// Setup GPIO pins from configuration
void setupGPIOPins(const std::unordered_map<std::string, std::string>& gpioPins) {
    if (gpioPins.empty()) {
        logMessage("INFO", "No GPIO pins configured");
        return;
    }
    
    logMessage("INFO", "Setting up GPIO pins:");
    for (const auto& [pinName, config] : gpioPins) {
        logMessage("INFO", "  " + pinName + " = " + config);
        // TODO: Actual GPIO setup with wiringPi or similar
    }
}

// Main service loop
void runService(const rpi::ServiceConfig& config) {
    logMessage("INFO", "Starting " + config.name + " service", config.logFile);
    logMessage("INFO", "Log level: " + config.logLevel, config.logFile);
    logMessage("INFO", "Configuration loaded successfully", config.logFile);
    
    // Get hostname for sensor output
    std::string hostname = rpi::getHostname();
    
    // Setup GPIO if enabled
    if (config.gpioEnabled) {
        logMessage("INFO", "GPIO support enabled", config.logFile);
        setupGPIOPins(config.gpioPins);
    }
    
    // Setup network if enabled
    if (config.networkEnabled) {
        logMessage("INFO", "Network enabled on port " + std::to_string(config.port), config.logFile);
        // TODO: Start network server
    }
    
    // Main loop
    int counter = 0;
    while (running) {
        // Sensor polling
        if (config.sensorLogging && counter % 10 == 0) {
            logMessage("DEBUG", "Polling sensors...", config.logFile);
        }
        
        // Read all DS18B20 temperature sensors
        std::vector<rpi::SensorReading> readings = rpi::readAllDSTemperatureSensors();
        
        // Print sensor readings in the format: hostname/sensor/id/measurement : value
        for (const auto& reading : readings) {
            std::cout << hostname << "/" << reading.sensor_type << "/" 
                      << reading.sensor_id << "/" << reading.measurement 
                      << " : " << std::fixed << std::setprecision(1) << reading.value << std::endl;
            
            // Also log to file if configured
            if (!config.logFile.empty() && config.sensorLogging) {
                logMessage("INFO", hostname + "/" + reading.sensor_type + "/" + 
                          reading.sensor_id + "/" + reading.measurement + " : " + 
                          std::to_string(reading.value), config.logFile);
            }
        }
        
        // Sleep for poll interval
        usleep(config.pollIntervalMs * 1000);
        counter++;
    }
    
    logMessage("INFO", "Service shutting down gracefully", config.logFile);
}

// Display configuration and exit (for testing)
void displayConfigAndExit(const rpi::ServiceConfig& config) {
    std::cout << "=== Rasens Service Configuration ===" << std::endl;
    std::cout << "Name: " << config.name << std::endl;
    std::cout << "Log Level: " << config.logLevel << std::endl;
    std::cout << "Log File: " << config.logFile << std::endl;
    std::cout << "PID File: " << config.pidFile << std::endl;
    std::cout << "Network Enabled: " << (config.networkEnabled ? "Yes" : "No") << std::endl;
    std::cout << "Port: " << config.port << std::endl;
    std::cout << "Bind Address: " << config.bindAddress << std::endl;
    std::cout << "GPIO Enabled: " << (config.gpioEnabled ? "Yes" : "No") << std::endl;
    std::cout << "Poll Interval: " << config.pollIntervalMs << "ms" << std::endl;
    std::cout << "Sensor Logging: " << (config.sensorLogging ? "Yes" : "No") << std::endl;
    
    std::cout << "\nGPIO Pins:" << std::endl;
    for (const auto& [pin, cfg] : config.gpioPins) {
        std::cout << "  " << pin << " = " << cfg << std::endl;
    }
    std::cout << "=====================================" << std::endl;
    exit(0);
}

int main(int argc, char* argv[]) {
    // Parse command line arguments
    bool showConfig = false;
    bool showInfo = false;
    bool daemonMode = false;
    
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--config" || arg == "-c") {
            showConfig = true;
        } else if (arg == "--info" || arg == "-i") {
            showInfo = true;
        } else if (arg == "--daemon" || arg == "-d") {
            daemonMode = true;
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: rasens [OPTIONS]" << std::endl;
            std::cout << "  --config, -c    Show configuration and exit" << std::endl;
            std::cout << "  --info, -i      Show system information and exit" << std::endl;
            std::cout << "  --daemon, -d    Run as daemon (background)" << std::endl;
            std::cout << "  --help, -h      Show this help message" << std::endl;
            return 0;
        }
    }
    
    // Show system info and exit if requested
    if (showInfo) {
        rpi::printSystemInfo();
        return 0;
    }
    
    // Load configuration
    rpi::ServiceConfig config = rpi::loadServiceConfig();
    
    // Show config and exit if requested
    if (showConfig) {
        displayConfigAndExit(config);
    }
    
    // Setup signal handlers for graceful shutdown
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    signal(SIGHUP, signalHandler);
    
    // Write PID file
    if (!config.pidFile.empty()) {
        ensureDirectoriesExist(config.pidFile);
        if (!writePidFile(config.pidFile)) {
            logMessage("ERROR", "Failed to write PID file: " + config.pidFile, config.logFile);
            return 1;
        }
        globalPidFile = config.pidFile;
        atexit(cleanupPidFile);
    }
    
    // Daemon mode: fork into background
    if (daemonMode) {
        pid_t pid = fork();
        if (pid < 0) {
            logMessage("ERROR", "Failed to fork", config.logFile);
            return 1;
        } else if (pid > 0) {
            // Parent process exits
            return 0;
        }
        
        // Child process continues
        setsid();
        umask(0);
        
        // Close standard file descriptors
        close(STDIN_FILENO);
        close(STDOUT_FILENO);
        close(STDERR_FILENO);
    }
    
    // Run the service
    runService(config);
    
    return 0;
}
