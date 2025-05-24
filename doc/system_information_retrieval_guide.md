# System Information Retrieval Guide

This document outlines the methods used in `system_monitor.cpp` to retrieve various system information on Linux systems, particularly on RK3588-based platforms like Orange Pi 5 Plus.

## Table of Contents
1. [Basic System Information](#basic-system-information)
2. [CPU Information](#cpu-information)
3. [GPU Information](#gpu-information)
4. [NPU Information](#npu-information)
5. [Memory Information](#memory-information)
6. [Storage Information](#storage-information)
7. [Network Information](#network-information)
8. [Power Information](#power-information)
9. [System Load](#system-load)

## Basic System Information

### Methods
- `updateBasicInfo()`: Retrieves basic system information
- `updateHardwareInfo()`: Retrieves hardware-specific information

### Implementation Details
1. **Hostname**
   - Uses `gethostname()` system call
   - Falls back to "unknown" if hostname cannot be determined

2. **Kernel Version**
   - Uses `uname()` system call to get kernel information
   - Extracts kernel release version

3. **OS Version**
   - Parses `/etc/os-release` file
   - Extracts PRETTY_NAME, NAME, and VERSION fields
   - Handles quoted values and different line formats
   - Falls back to uname information if file is not available

4. **System Uptime**
   - Reads `/proc/uptime`
   - Converts uptime in seconds to days, hours, minutes, seconds
   - Formats as "Xd HH:MM:SS" or "HH:MM:SS" if less than a day

5. **System Time**
   - Uses C++ `std::chrono` and `std::time`
   - Formats as "YYYY-MM-DD HH:MM:SS"

### Data Sources
- `/etc/os-release` for distribution information
- `/proc/uptime` for system uptime
- System calls: `gethostname()`, `uname()`
- C++ standard library for time handling

### Commands Used
- File I/O operations to read system files
- No external commands used

### Error Handling
- Provides fallback values for all information
- Logs debug information for troubleshooting
- Handles missing or malformed files gracefully

## CPU Information

### Methods
- `updateCpuInfo()`: Retrieves CPU usage, temperature, and frequency

### Implementation Details
1. **CPU Usage Calculation**
   - Reads `/proc/stat` to get CPU time statistics
   - Calculates usage percentage by comparing current and previous readings
   - Handles both overall CPU usage and per-core usage
   - Implements a 500ms timeout for CPU information retrieval

2. **CPU Temperature**
   - Reads from multiple thermal zones:
     - `/sys/class/thermal/thermal_zone1/temp` (Big core 0)
     - `/sys/class/thermal/thermal_zone2/temp` (Big core 1)
     - `/sys/class/thermal/thermal_zone3/temp` (Little core)
     - `/sys/class/thermal/thermal_zone0/temp` (SoC temperature)
   - Converts millidegree Celsius to degrees Celsius by dividing by 1000
   - Falls back to default 40°C if temperature reading fails

3. **CPU Frequency**
   - Reads from `/sys/devices/system/cpu/cpu*/cpufreq/scaling_cur_freq`
   - Averages frequencies across all cores
   - Falls back to 1.8GHz if frequency reading fails

### Commands Used
- File I/O operations to read system files
- No external commands used (pure C++ implementation)

### Error Handling
- Logs errors if files cannot be opened
- Handles malformed data gracefully
- Implements timeouts to prevent hanging

### Dependencies
- Requires access to `/proc` and `/sys` filesystems

## GPU Information

### Methods
- `updateGpuInfo()`: Retrieves GPU usage, temperature, and memory information

### Implementation Details
1. **GPU Temperature**
   - Primarily reads from `/sys/class/thermal/thermal_zone5/temp`
   - Falls back to CPU temperature if GPU temperature is unavailable
   - Uses a command to search for GPU thermal zone: 
     ```bash
     for i in /sys/class/thermal/thermal_zone*/type; do 
       if grep -q -i gpu "$i"; then 
         zone=${i%/type}; cat "$zone/temp" 2>/dev/null; 
         break; 
       fi; 
     done
     ```

2. **GPU Load**
   - Reads from `/sys/class/devfreq/fb000000.gpu/load`
   - Converts the load value to percentage (divides by 10)
   - Falls back to 0% if reading fails

3. **GPU Frequency**
   - Reads from `/sys/devices/platform/fb000000.gpu/clock`
   - Converts Hz to MHz for display

4. **GPU Memory Usage**
   - Estimates memory usage as 80% of GPU load (approximation)
   - No direct memory usage information available in sysfs

### Commands Used
- `cat` for reading thermal zone information
- File I/O operations for reading sysfs entries

### Error Handling
- Falls back to CPU temperature if GPU temperature is unavailable
- Logs debug messages for troubleshooting
- Handles missing files gracefully

### Dependencies
- Requires Mali GPU kernel module to be loaded
- Requires root access for some metrics

## NPU Information

### Methods
- `updateNpuInfo()`: Retrieves NPU usage and temperature

### Data Sources
- **NPU Load**: `/sys/kernel/debug/rknpu/load`
- **NPU Temperature**: Same as GPU temperature (approximation)

### Dependencies
- Requires RKNPU kernel module to be loaded
- Requires debugfs to be mounted

## Memory Information

### Methods
- `updateMemoryInfo()`: Retrieves memory and swap usage

### Implementation Details
1. **Memory Usage**
   - Parses `/proc/meminfo` to get memory statistics
   - Converts values from KB to bytes for consistency
   - Calculates used memory as: `MemTotal - MemAvailable`
   - Computes memory usage percentage: `100 * (MemTotal - MemAvailable) / MemTotal`

2. **Storage Information**
   - Reads `/proc/mounts` to get mounted filesystems
   - Filters for common filesystem types (ext4, xfs, btrfs, etc.)
   - Uses `statvfs()` to get filesystem statistics
   - Calculates usage percentage for each filesystem

### Data Sources
- `/proc/meminfo` for memory statistics
- `/proc/mounts` for mounted filesystems
- `statvfs()` system call for filesystem statistics

### Commands Used
- File I/O operations to read system files
- No external commands used (pure C++ implementation)

### Error Handling
- Logs errors if files cannot be opened
- Handles malformed input gracefully
- Skips invalid mount points

## Storage Information

### Methods
- `updateStorageInfo()`: Retrieves storage device information

### Data Sources
- **Mounted Filesystems**: `/proc/mounts`
- **Filesystem Statistics**: `statvfs()`

### Information Retrieved
- Mount point
- Total space
- Used space
- Available space
- Usage percentage
- Filesystem type

## Network Information

### Methods
- `updateNetworkInfo()`: Retrieves network interface information and statistics

### Implementation Details
1. **Network Interfaces**
   - Uses `getifaddrs()` to enumerate network interfaces
   - Skips loopback interface (lo)
   - Extracts IPv4 addresses using `inet_ntop()`

2. **Network Statistics**
   - Reads `/proc/net/dev` for interface statistics
   - Parses RX/TX bytes, packets, errors, drops, etc.
   - Calculates network rates by comparing with previous readings
   - Handles interface renaming and new interfaces

3. **WiFi Information**
   - Uses `nmcli` command to get WiFi SSID (if NetworkManager is available)
   - Falls back to parsing system logs if direct method fails

### Data Sources
- `/proc/net/dev` for network statistics
- `getifaddrs()` for interface enumeration
- `nmcli` for WiFi information (if available)

### Commands Used
- `nmcli -t -f active,ssid dev wifi | grep '^yes' | cut -d: -f2` for WiFi SSID
- `ip route show default` for default gateway
- `ip -o -4 addr show <interface> | awk '{print $4}'` for subnet mask
- `cat /sys/class/net/<interface>/address` for MAC address

### Error Handling
- Handles missing interfaces gracefully
- Logs errors for troubleshooting
- Falls back to default values when information is unavailable

### Information Retrieved
- Interface name
- IP address
- MAC address
- Network mask
- Gateway
- Bytes received/transmitted
- Packets received/transmitted
- Connection status
- WiFi SSID (for wireless interfaces)

### Dependencies
- Requires `nmcli` for WiFi SSID information
- Requires root access for some network statistics

## Power Information

### Methods
- `updatePowerInfo()`: Retrieves power-related information

### Implementation Details
1. **Power Source Detection**
   - Executes: `cat /sys/class/power_supply/*/type 2>/dev/null | grep -q Battery && echo 'Battery' || echo 'AC'`
   - Determines if system is on battery or AC power

2. **Battery Information**
   - Reads battery capacity: `cat /sys/class/power_supply/*/capacity 2>/dev/null`
   - Reads battery status: `cat /sys/class/power_supply/*/status 2>/dev/null`
   - Handles both single and multiple battery systems

3. **Power Management**
   - Reads CPU governor: `cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor 2>/dev/null`
   - Determines power mode based on governor settings

### Data Sources
- `/sys/class/power_supply/` for battery and power source information
- `/sys/devices/system/cpu/cpu0/cpufreq/` for CPU frequency governor

### Commands Used
- `cat` for reading sysfs entries
- Shell pipelines for power source detection

### Error Handling
- Falls back to "AC" power if battery information is unavailable
- Handles missing sysfs entries gracefully
- Logs debug information for troubleshooting

### Information Retrieved
- Power source (AC/Battery)
- Battery percentage
- Battery status (Charging/Discharging)
- Power mode (Performance/Powersave)

## System Load

### Methods
- `updateLoadAverage()`: Retrieves system load averages

### Implementation Details
1. **Load Average**
   - Reads the first three values from `/proc/loadavg`
   - Values represent 1-minute, 5-minute, and 15-minute load averages
   - Load average represents the average number of processes in the run queue

### Data Sources
- `/proc/loadavg` for system load averages

### Commands Used
- File I/O operations to read `/proc/loadavg`
- No external commands used

### Error Handling
- Logs errors if the file cannot be opened
- Initializes load averages to 0.0 if reading fails

### Information Retrieved
- 1-minute load average
- 5-minute load average
- 15-minute load average

## Error Handling

All methods include error handling for:
- Missing files or directories
- Permission issues
- Invalid data formats
- Timeouts for potentially slow operations

## Dependencies

- Linux kernel (2.6.18+ recommended)
- Standard C++ library
- POSIX libraries
- NetworkManager (for WiFi SSID)
- RKNPU kernel module (for NPU information)
- Mali kernel module (for GPU information)

## Notes

1. Some metrics require root privileges for full functionality
2. Hardware-specific paths may vary between different ARM SoCs
3. Temperature readings may need calibration for accuracy
4. Network statistics depend on kernel network stack implementation
5. Power management features depend on hardware and kernel support
