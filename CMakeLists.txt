cmake_minimum_required(VERSION 3.10)
project(rasens PROJECT_VERSION "1.0.0" LANGUAGES CXX)

# Use C++17 standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Project structure
set(SOURCE_DIR "src")
set(INCLUDE_DIR "include")
set(CONFIG_DIR "config")
set(SYSTEMD_DIR "systemd")

# Find all source files
file(GLOB SOURCES "${SOURCE_DIR}/*.cpp")
file(GLOB HEADERS "${INCLUDE_DIR}/*.hpp" "${INCLUDE_DIR}/*.h")

# Executable
add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

# Include directories
target_include_directories(${PROJECT_NAME} PRIVATE ${INCLUDE_DIR})

# Raspberry Pi specific: Link with common libraries
# Uncomment what you need:
# find_package(WiringPi REQUIRED)
# target_link_libraries(${PROJECT_NAME} PRIVATE WiringPi::WiringPi)
# target_link_libraries(${PROJECT_NAME} PRIVATE pigpio)
target_link_libraries(${PROJECT_NAME} PRIVATE pthread)

# Installation paths
set(INSTALL_BIN_DIR "/opt/rasens/bin" CACHE PATH "Binary installation directory")
set(INSTALL_CONFIG_DIR "/etc/rasens" CACHE PATH "Configuration installation directory")
set(INSTALL_SYSTEMD_DIR "/etc/systemd/system" CACHE PATH "Systemd service directory")
set(INSTALL_LOG_DIR "/var/log/rasens" CACHE PATH "Log directory")
set(INSTALL_RUN_DIR "/var/run" CACHE PATH "Runtime directory")

# Install executable
install(TARGETS ${PROJECT_NAME} 
    DESTINATION ${INSTALL_BIN_DIR}
    PERMISSIONS OWNER_EXECUTE OWNER_WRITE OWNER_READ GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
)

# Install configuration file
install(FILES ${CONFIG_DIR}/rasens.conf 
    DESTINATION ${INSTALL_CONFIG_DIR}
    RENAME config.conf
    PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ
)

# Install systemd service file
install(FILES ${SYSTEMD_DIR}/rasens.service 
    DESTINATION ${INSTALL_SYSTEMD_DIR}
    PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ
)

# Create directories (not handled by cmake install, manual step)
# This message reminds users to create these directories
message(STATUS "Remember to create directories manually or use the install script:")
message(STATUS "  sudo mkdir -p ${INSTALL_BIN_DIR}")
message(STATUS "  sudo mkdir -p ${INSTALL_CONFIG_DIR}")
message(STATUS "  sudo mkdir -p ${INSTALL_LOG_DIR}")

# Option to build with debug symbols
option(BUILD_DEBUG "Build with debug symbols" OFF)
if(BUILD_DEBUG)
    add_compile_options(-g -DDEBUG)
    message(STATUS "Building with debug symbols")
endif()

# Enable testing
option(BUILD_TESTS "Build tests" OFF)
if(BUILD_TESTS)
    enable_testing()
    # Add test executable here if needed
endif()
