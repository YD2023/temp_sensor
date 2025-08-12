# ESP32 Temperature & Humidity Sensor with BLE Beacon

Hello! This project was a path for me to delve deeper into using FreeRTOS, I2C protocols, and BLE concepts in a real world setting. I created a comprehensive IoT temperature and humidity monitoring system built on the ESP32 platform using ESP-IDF framework. This project combines DHT22 sensor readings, OLED display output, and Bluetooth Low Energy (BLE) beacon functionality for wireless data transmission.

## 🔍 Overview

This project continuously reads temperature and humidity data from a DHT22 sensor, displays the information on an SSD1306 OLED screen, and broadcasts the temperature data via BLE beacon using the Eddystone TLM (Telemetry) format.

### Key Capabilities:
- **Real-time Environmental Monitoring**: Continuous temperature and humidity sensing
- **Visual Display**: Local data visualization on OLED screen
- **Wireless Data Broadcasting**: BLE beacon transmission for remote monitoring
- **Multi-tasking Architecture**: Efficient FreeRTOS-based task management
- **Data Buffering**: Ring buffer implementation for sensor data history

## 📁 Videos
- **Example**: Below is the project in use as well as BLE readings that were recognized through the RF connect app.

[![Watch the video](https://img.youtube.com/vi/1sJHljRCxrs/maxresdefault.jpg)](https://youtu.be/1sJHljRCxrs)

## 🛠 Hardware Requirements

### Essential Components:
- **ESP32 Development Board** (ESP32-DevKitC or compatible)
- **DHT22 Temperature & Humidity Sensor**
- **SSD1306 OLED Display** (128x64 pixels, I2C interface)
- **Breadboard and Jumper Wires**
- **Power Supply** (5V USB or 3.3V regulated)

### Optional Components:
- **Pull-up Resistor** (4.7kΩ for DHT22 data line, if not built into sensor module)
- **Enclosure** for weatherproofing (if used outdoors)

## 🏗 Software Architecture

The project is built using the ESP-IDF framework with PlatformIO as the development platform. It leverages several key libraries:

### Core Dependencies:
- **ESP-IDF**: Espressif's official development framework
- **FreeRTOS**: Real-time operating system for task management
- **NimBLE**: Lightweight Bluetooth Low Energy stack
- **DHT22 Library**: Sensor communication driver
- **SSD1306 Driver**: OLED display interface

### Development Tools:
- **PlatformIO**: Cross-platform IDE and build system
- **CMake**: Build system configuration
- **ESP-IDF Tools**: Compilation and flashing utilities

## ✨ Features

### 🌡️ Environmental Sensing
- **Temperature Measurement**: Accurate readings from DHT22 sensor
- **Humidity Monitoring**: Relative humidity percentage tracking
- **Sub 1-second Update Interval**: Real-time data refresh

### 📺 Display Interface
- **OLED Visualization**: 128x64 pixel SSD1306 display
- **I2C**: Utilized I2C communication between controller and display
- **Multi-line Information**: Temperature, humidity, and status messages
- **Clear Text Rendering**: 8x8 pixel font for readability

### 📡 Bluetooth Low Energy
- **Eddystone TLM Beacon**: Industry-standard telemetry format
- **Temperature Broadcasting**: Real-time temperature data transmission
- **Continuous Advertisement**: Automatic beacon restart

### ⚡ System Management
- **Multi-task Architecture**: Parallel processing of sensor, display, and BLE operations
- **Data Synchronization**: Queue and ring buffer for inter-task communication
- **Semaphore Coordination**: Thread-safe data access
- **Event-driven Initialization**: Synchronized startup sequence

## 📌 Pin Configuration

### DHT22 Sensor Connections:
```
DHT22 VCC  → ESP32 3.3V
DHT22 GND  → ESP32 GND
DHT22 DATA → ESP32 GPIO 4
```

### SSD1306 OLED Display (I2C):
```
OLED VCC → ESP32 3.3V
OLED GND → ESP32 GND
OLED SCL → ESP32 GPIO 22 (I2C Clock)
OLED SDA → ESP32 GPIO 21 (I2C Data)
```

### Power Supply:
```
ESP32 VIN → 5V USB Power
ESP32 GND → Ground
```

## 🚀 Installation & Setup

### Prerequisites:
1. **Install PlatformIO**: Download from [platformio.org](https://platformio.org/)
2. **ESP-IDF Setup**: PlatformIO will automatically handle ESP-IDF installation
3. **USB Drivers**: Install appropriate drivers for your ESP32 board

### Project Setup:
```bash
# Clone or download the project
git clone <repository-url>
cd temp_sensor

# Open in PlatformIO
# The platformio.ini file contains all necessary configurations
```

### Library Dependencies:
The project uses the following libraries (automatically managed by PlatformIO):
```ini
lib_deps = 
    andrey-m/DHT22 C|C++ library for ESP32 (ESP-IDF)@^1.0.6
```

## 🔨 Building and Flashing

### Using PlatformIO:
```bash
# Build the project
pio run

# Flash to ESP32
pio run --target upload

# Monitor serial output
pio device monitor
```


## 📡 BLE Beacon Protocol

### Eddystone TLM Format:
The system broadcasts temperature data using the Eddystone Telemetry (TLM) frame format:

```
Frame Structure:
- Service UUID: 0xFEAA (Eddystone)
- Frame Type: 0x20 (TLM)
- Version: 0x00
- Battery Voltage: N/A (0x0000)
- Temperature: [2 bytes, fixed-point format]
- Advertisement Count: N/A
- Uptime: N/A
```

### Temperature Encoding:
- **Format**: 16-bit signed integer
- **Resolution**: 1/256°C per bit
- **Range**: -128°C to +127.99°C
- **Conversion**: Temperature × 256

### Device Discovery:
- **Device Name**: "TempSensor"
- **Advertisement Interval**: Continuous
- **Scan Response**: Contains device identification


## ⚙️ FreeRTOS Task Management

### Task Architecture:
The system implements three main FreeRTOS tasks:

#### 1. DHT_task (Priority: 3)
- **Function**: Sensor data acquisition
- **Frequency**: 1-second intervals
- **Operations**:
  - Read DHT22 sensor
  - Convert temperature units
  - Send data to queue
  - Error handling

#### 2. process_display_task (Priority: 4)
- **Function**: Data processing and display
- **Operations**:
  - Receive data from queue
  - Store in ring buffer
  - Update OLED display
  - Signal BLE task

#### 3. ble_beacon_task (Priority: 5)
- **Function**: BLE advertisement management
- **Operations**:
  - Wait for new data signal
  - Retrieve latest sensor data
  - Update BLE advertisement
  - Restart beacon transmission

### Synchronization Mechanisms:
- **Queue**: Inter-task data transfer
- **Ring Buffer**: Data history storage
- **Semaphore**: Task notification
- **Event Groups**: Initialization coordination

