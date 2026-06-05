#include <iostream>
#include <string>
#include <fstream>
#include <csignal>
#include <cstdlib>
#include <cstdio>
#include <sys/stat.h>
#include <unistd.h>
#include <iomanip>
#include <vector>
#include <memory>
#include <mutex>

#include "config.hpp"
#include "utils.hpp"
#include "scanner.hpp"

static volatile sig_atomic_t running = 1;
static volatile sig_atomic_t reloadRequested = 0;
static std::string globalPidFile;

static std::ofstream g_logStream;
static std::mutex g_logMutex;

// forward declaration — defined below
void ensureDirectoriesExist(const std::string& path);

void openLogFile(const std::string& path) {
    if (path.empty()) return;
    ensureDirectoriesExist(path);
    std::lock_guard<std::mutex> lock(g_logMutex);
    if (g_logStream.is_open()) g_logStream.close();
    g_logStream.open(path, std::ios::app);
}

void signalHandler(int) {
    running = 0;
}

void sighupHandler(int) {
    reloadRequested = 1;
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

// Create directories if they don't exist (equivalent to mkdir -p)
void ensureDirectoriesExist(const std::string& path) {
    size_t pos = path.find_last_of('/');
    if (pos == std::string::npos) return;
    std::string dir = path.substr(0, pos);
    for (size_t i = 1; i <= dir.size(); ++i) {
        if (i == dir.size() || dir[i] == '/') {
            mkdir(dir.substr(0, i).c_str(), 0755);
        }
    }
}

static std::string formatValue(float v) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%g", v);
    return buf;
}

void logMessage(const std::string& level, const std::string& message) {
    std::string formatted = "[" + level + "] " + message;
    std::cout << formatted << "\n";

    std::lock_guard<std::mutex> lock(g_logMutex);
    if (g_logStream.is_open()) {
        g_logStream << formatted << "\n";
        g_logStream.flush();
    }
}

static void printReading(const std::string& hostname, const rpi::SensorReading& r, bool logEnabled) {
    std::string line = "sensors/" + hostname + "/" + r.sensor_type + "/" + r.sensor_id + "/" + r.measurement
                     + " : " + formatValue(r.value);
    std::cout << line << "\n";
    if (logEnabled) {
        std::lock_guard<std::mutex> lock(g_logMutex);
        if (g_logStream.is_open()) {
            g_logStream << line << "\n";
            g_logStream.flush();
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
void runService(rpi::ServiceConfig config, bool runOnce, bool stdoutOnly) {
    std::string hostname = rpi::getHostname();

    // Open persistent log file (single open, not per-write)
    if (!stdoutOnly && !config.logFile.empty())
        openLogFile(config.logFile);

    if (!runOnce) {
        logMessage("INFO", "Starting " + config.name + " service");
        logMessage("INFO", "Log level: " + config.logLevel);
        logMessage("INFO", "Configuration loaded successfully");
        if (config.gpioEnabled) {
            logMessage("INFO", "GPIO support enabled");
            setupGPIOPins(config.gpioPins);
        }
        if (config.networkEnabled)
            logMessage("INFO", "Network enabled on port " + std::to_string(config.port));
    }

    // Start SDS011 reader threads — one per device
    std::vector<std::unique_ptr<rpi::SDS011Reader>> sds011Readers;
    auto initSDS011Readers = [&]() {
        sds011Readers.clear();
        if (!config.sds011Enabled) return;
        for (const auto& dev : config.sds011Devices)
            sds011Readers.push_back(std::make_unique<rpi::SDS011Reader>(dev));
    };
    initSDS011Readers();

    // Start VE.Direct reader threads — one per device
    std::vector<std::unique_ptr<rpi::VEDirectReader>> vedirectReaders;
    auto initVEDirectReaders = [&]() {
        vedirectReaders.clear();
        if (!config.vedirectEnabled) return;
        for (size_t i = 0; i < config.vedirectDevices.size(); ++i) {
            const std::string& dev = config.vedirectDevices[i];
            std::string id = (i < config.vedirectSensorIds.size())
                ? config.vedirectSensorIds[i]
                : dev.substr(dev.find_last_of('/') + 1);
            vedirectReaders.push_back(std::make_unique<rpi::VEDirectReader>(dev, id));
        }
    };
    initVEDirectReaders();

    // In --once mode, wait up to 3s for threaded readers to receive their first frame
    if (runOnce) {
        for (auto& reader : sds011Readers)
            reader->getReadings(); // non-blocking, just primes
        for (auto& reader : vedirectReaders)
            reader->waitForReading(3000);
    }

    unsigned int counter = 0;
    do {
        if (reloadRequested) {
            config = rpi::loadServiceConfig();
            reloadRequested = 0;
            if (!stdoutOnly)
                openLogFile(config.logFile);
            initSDS011Readers();
            initVEDirectReaders();
            logMessage("INFO", "Configuration reloaded");
        }

        bool logToFile = config.sensorLogging && !stdoutOnly && g_logStream.is_open();

        if (config.ds18b20Enabled)
            for (const auto& r : rpi::readAllDSTemperatureSensors())
                printReading(hostname, r, logToFile);

        if (config.ee895Enabled)
            for (const auto& r : rpi::readEE895Sensor(config.ee895I2CBus, config.ee895I2CAddress, config.ee895SensorId))
                printReading(hostname, r, logToFile);

        for (auto& reader : sds011Readers)
            for (const auto& r : reader->getReadings())
                printReading(hostname, r, logToFile);

        if (config.ina219Enabled)
            for (const auto& r : rpi::readINA219Sensor(config.ina219I2CBus, config.ina219I2CAddress,
                                                        config.ina219SensorId, config.ina219ShuntResistance))
                printReading(hostname, r, logToFile);

        for (auto& reader : vedirectReaders)
            for (const auto& r : reader->getReadings())
                printReading(hostname, r, logToFile);

        if (!runOnce) usleep(config.pollIntervalMs * 1000);
        counter++;
    } while (!runOnce && running);

    if (!runOnce)
        logMessage("INFO", "Service shutting down gracefully");
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
    std::cout << "DS18B20 Enabled: " << (config.ds18b20Enabled ? "Yes" : "No") << std::endl;
    std::cout << "EE895 Enabled: " << (config.ee895Enabled ? "Yes" : "No") << std::endl;
    if (config.ee895Enabled) {
        std::cout << "  I2C Bus: " << config.ee895I2CBus << std::endl;
        std::cout << "  I2C Address: 0x" << std::hex << config.ee895I2CAddress << std::dec << std::endl;
        std::cout << "  Sensor ID: " << config.ee895SensorId << std::endl;
    }
    std::cout << "SDS011 Enabled: " << (config.sds011Enabled ? "Yes" : "No") << std::endl;
    if (config.sds011Enabled) {
        for (const auto& dev : config.sds011Devices)
            std::cout << "  Device: " << dev << std::endl;
    }
    std::cout << "INA219 Enabled: " << (config.ina219Enabled ? "Yes" : "No") << std::endl;
    if (config.ina219Enabled) {
        std::cout << "  I2C Bus: " << config.ina219I2CBus << std::endl;
        std::cout << "  I2C Address: 0x" << std::hex << config.ina219I2CAddress << std::dec << std::endl;
        std::cout << "  Sensor ID: " << config.ina219SensorId << std::endl;
        std::cout << "  Shunt Resistance: " << config.ina219ShuntResistance << " Ohm" << std::endl;
    }
    std::cout << "VEDirect Enabled: " << (config.vedirectEnabled ? "Yes" : "No") << std::endl;
    if (config.vedirectEnabled) {
        for (size_t i = 0; i < config.vedirectDevices.size(); ++i) {
            std::cout << "  Device: " << config.vedirectDevices[i];
            if (i < config.vedirectSensorIds.size())
                std::cout << " (id: " << config.vedirectSensorIds[i] << ")";
            std::cout << std::endl;
        }
    }
    std::cout << "\nGPIO Pins:" << std::endl;
    for (const auto& [pin, cfg] : config.gpioPins)
        std::cout << "  " << pin << " = " << cfg << std::endl;
    std::cout << "=====================================" << std::endl;
    exit(0);
}

int main(int argc, char* argv[]) {
    // Parse command line arguments
    bool showConfig = false;
    bool showInfo = false;
    bool daemonMode = false;
    bool doScan = false;
    bool runOnce = false;
    bool stdoutOnly = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--config" || arg == "-c") {
            showConfig = true;
        } else if (arg == "--info" || arg == "-i") {
            showInfo = true;
        } else if (arg == "--daemon" || arg == "-d") {
            daemonMode = true;
        } else if (arg == "--scan" || arg == "-s") {
            doScan = true;
        } else if (arg == "--once") {
            runOnce = true;
        } else if (arg == "--stdout") {
            stdoutOnly = true;
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: rasens [OPTIONS]" << std::endl;
            std::cout << "  --config, -c    Show configuration and exit" << std::endl;
            std::cout << "  --info, -i      Show system information and exit" << std::endl;
            std::cout << "  --daemon, -d    Run as daemon (background)" << std::endl;
            std::cout << "  --scan, -s      Detect sensors and write config, then exit" << std::endl;
            std::cout << "  --once          Read all sensors once and exit" << std::endl;
            std::cout << "  --stdout        Print to stdout only, suppress file logging" << std::endl;
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

    // Scan sensors, update and write config, then exit
    if (doScan) {
        rpi::scanAndUpdateConfig(config);
        if (rpi::writeServiceConfig(config)) {
            std::cout << "[SCAN] Config written to " << config.configPath << std::endl;
        }
        return 0;
    }

    // Show config and exit if requested
    if (showConfig) {
        displayConfigAndExit(config);
    }
    
    // Setup signal handlers for graceful shutdown
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    signal(SIGHUP, sighupHandler);

    // Daemon mode: fork into background before writing PID file
    if (daemonMode) {
        pid_t pid = fork();
        if (pid < 0) {
            logMessage("ERROR", "Failed to fork");
            return 1;
        } else if (pid > 0) {
            // Parent exits without touching the PID file
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

    // Write PID file after fork so only the running process owns it
    if (!config.pidFile.empty()) {
        ensureDirectoriesExist(config.pidFile);
        if (!writePidFile(config.pidFile)) {
            logMessage("ERROR", "Failed to write PID file: " + config.pidFile);
            return 1;
        }
        globalPidFile = config.pidFile;
        atexit(cleanupPidFile);
    }
    
    runService(config, runOnce, stdoutOnly);
    
    return 0;
}
