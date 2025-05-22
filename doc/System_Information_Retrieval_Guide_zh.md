# 系统信息检索指南

本文档概述了在`system_monitor.cpp`中用于在Linux系统（特别是基于RK3588的平台，如Orange Pi 5 Plus）上检索各种系统信息的方法。

## 目录
1. [基本系统信息](#基本系统信息)
2. [CPU信息](#cpu信息)
3. [GPU信息](#gpu信息)
4. [NPU信息](#npu信息)
5. [内存信息](#内存信息)
6. [存储信息](#存储信息)
7. [网络信息](#网络信息)
8. [电源信息](#电源信息)
9. [系统负载](#系统负载)
10. [错误处理](#错误处理)
11. [依赖项](#依赖项)
12. [注意事项](#注意事项)

## 基本系统信息

### 方法
- `updateBasicInfo()`: 检索基本系统信息
- `updateHardwareInfo()`: 检索硬件特定信息

### 实现细节
1. **主机名**
   - 使用`gethostname()`系统调用
   - 无法确定时回退到"unknown"

2. **内核版本**
   - 使用`uname()`系统调用获取内核信息
   - 提取内核发布版本

3. **操作系统版本**
   - 解析`/etc/os-release`文件
   - 提取PRETTY_NAME、NAME和VERSION字段
   - 处理带引号的值和不同行格式
   - 文件不可用时回退到uname信息

4. **系统运行时间**
   - 读取`/proc/uptime`
   - 将秒数转换为天、小时、分钟、秒
   - 格式化为"Xd HH:MM:SS"或不足一天时"HH:MM:SS"

5. **系统时间**
   - 使用C++ `std::chrono`和`std::time`
   - 格式化为"YYYY-MM-DD HH:MM:SS"

### 数据源
- `/etc/os-release` 发行版信息
- `/proc/uptime` 系统运行时间
- 系统调用：`gethostname()`, `uname()`
- C++标准库处理时间

### 使用的命令
- 文件I/O操作读取系统文件
- 无外部命令

### 错误处理
- 为所有信息提供回退值
- 记录调试信息便于故障排除
- 优雅处理缺失或格式错误的文件

## CPU信息

### 方法
- `updateCpuInfo()`: 检索CPU使用率、温度和频率

### 实现细节
1. **CPU使用率计算**
   - 读取`/proc/stat`获取CPU时间统计
   - 通过比较当前和之前读数计算使用率百分比
   - 支持整体CPU使用率和每个核心使用率
   - 实现500毫秒超时机制

2. **CPU温度**
   - 从多个温度区域读取：
     - `/sys/class/thermal/thermal_zone1/temp` (大核0)
     - `/sys/class/thermal/thermal_zone2/temp` (大核1)
     - `/sys/class/thermal/thermal_zone3/temp` (小核)
     - `/sys/class/thermal/thermal_zone0/temp` (SoC温度)
   - 将毫摄氏度转换为摄氏度(除以1000)
   - 读取失败时回退到默认值40°C

3. **CPU频率**
   - 从`/sys/devices/system/cpu/cpu*/cpufreq/scaling_cur_freq`读取
   - 计算所有核心的平均频率
   - 读取失败时使用1.8GHz作为默认值

### 使用的命令
- 文件I/O操作读取系统文件
- 无外部命令（纯C++实现）

### 错误处理
- 文件无法打开时记录错误
- 优雅处理格式错误的数据
- 实现超时机制防止卡死

### 依赖项
- 需要访问`/proc`和`/sys`文件系统

## GPU信息

### 方法
- `updateGpuInfo()`: 检索GPU使用率、温度和内存信息

### 实现细节
1. **GPU温度**
   - 主要从`/sys/class/thermal/thermal_zone5/temp`读取
   - 如果GPU温度不可用，则回退到CPU温度
   - 使用命令搜索GPU温度区域：
     ```bash
     for i in /sys/class/thermal/thermal_zone*/type; do 
       if grep -q -i gpu "$i"; then 
         zone=${i%/type}; cat "$zone/temp" 2>/dev/null; 
         break; 
       fi; 
     done
     ```

2. **GPU负载**
   - 从`/sys/class/devfreq/fb000000.gpu/load`读取
   - 将负载值转换为百分比(除以10)
   - 读取失败时使用0%

3. **GPU频率**
   - 从`/sys/devices/platform/fb000000.gpu/clock`读取
   - 将Hz转换为MHz显示

4. **GPU内存使用率**
   - 估算为GPU负载的80%(近似值)
   - sysfs中没有直接的内存使用信息

### 使用的命令
- 使用`cat`读取温度区域信息
- 文件I/O操作读取sysfs条目

### 错误处理
- GPU温度不可用时回退到CPU温度
- 记录调试信息便于故障排除
- 优雅处理缺失的文件

### 依赖项
- 需要加载Mali GPU内核模块
- 某些指标需要root权限

## NPU信息

### 方法
- `updateNpuInfo()`: 检索NPU使用率和温度

### 数据源
- **NPU负载**: `/sys/kernel/debug/rknpu/load`

```shell
orangepi@orangepi5plus:~/Qworkspace/cam_server_cpp$ sudo cat /sys/kernel/debug/rknpu/load
NPU load:  Core0:  0%, Core1:  0%, Core2:  0%,
orangepi@orangepi5plus:~/Qworkspace/cam_server_cpp$ 
```

- **NPU温度**: 与GPU温度相同（近似值）

### 依赖项
- 需要加载RKNPU内核模块
- 需要挂载debugfs

## 内存信息

### 方法
- `updateMemoryInfo()`: 检索内存和交换空间使用情况

### 实现细节
1. **内存使用**
   - 解析`/proc/meminfo`获取内存统计信息
   - 将KB转换为字节以保持一致性
   - 计算已用内存：`MemTotal - MemAvailable`
   - 计算内存使用百分比：`100 * (MemTotal - MemAvailable) / MemTotal`

2. **存储信息**
   - 读取`/proc/mounts`获取已挂载的文件系统
   - 过滤常见文件系统类型(ext4, xfs, btrfs等)
   - 使用`statvfs()`获取文件系统统计信息
   - 计算每个文件系统的使用百分比

### 数据源
- `/proc/meminfo` 内存统计
- `/proc/mounts` 已挂载的文件系统
- `statvfs()` 系统调用获取文件系统统计

### 使用的命令
- 文件I/O操作读取系统文件
- 无外部命令(纯C++实现)

### 错误处理
- 文件无法打开时记录错误
- 优雅处理格式错误的输入
- 跳过无效的挂载点

## 存储信息

### 方法
- `updateStorageInfo()`: 检索存储设备信息

### 数据源
- **挂载的文件系统**: `/proc/mounts`
- **文件系统统计信息**: `statvfs()`

### 检索到的信息
- 挂载点
- 总空间
- 已用空间
- 可用空间
- 使用百分比
- 文件系统类型

## 网络信息

### 方法
- `updateNetworkInfo()`: 检索网络接口信息和统计信息

### 实现细节
1. **网络接口**
   - 使用`getifaddrs()`枚举网络接口
   - 跳过回环接口(lo)
   - 使用`inet_ntop()`提取IPv4地址

2. **网络统计**
   - 从`/proc/net/dev`读取接口统计信息
   - 解析接收/发送的字节数、数据包数、错误数、丢包数等
   - 通过比较之前读数计算网络速率
   - 处理接口重命名和新接口

3. **WiFi信息**
   - 使用`nmcli`命令获取WiFi SSID(如果NetworkManager可用)
   - 直接方法失败时回退到解析系统日志

### 数据源
- `/proc/net/dev` 网络统计
- `getifaddrs()` 接口枚举
- `nmcli` WiFi信息(如果可用)

### 使用的命令
- `nmcli -t -f active,ssid dev wifi | grep '^yes' | cut -d: -f2` 获取WiFi SSID
- `ip route show default` 获取默认网关
- `ip -o -4 addr show <interface> | awk '{print $4}'` 获取子网掩码
- `cat /sys/class/net/<interface>/address` 获取MAC地址

### 错误处理
- 优雅处理缺失的接口
- 记录错误信息便于故障排除
- 信息不可用时回退到默认值

### 检索到的信息
- 接口名称
- IP地址
- MAC地址
- 网络掩码
- 网关
- 接收/发送的字节数
- 接收/发送的数据包数
- 连接状态
- WiFi SSID（针对无线接口）

### 依赖项
- 需要`nmcli`获取WiFi SSID信息
- 某些网络统计信息需要root权限

## 电源信息

### 方法
- `updatePowerInfo()`: 检索电源相关信息

### 实现细节
1. **电源检测**
   - 执行命令：`cat /sys/class/power_supply/*/type 2>/dev/null | grep -q Battery && echo 'Battery' || echo 'AC'`
   - 判断系统是使用电池还是交流电源

2. **电池信息**
   - 读取电池电量：`cat /sys/class/power_supply/*/capacity 2>/dev/null`
   - 读取电池状态：`cat /sys/class/power_supply/*/status 2>/dev/null`
   - 处理单电池和多电池系统

3. **电源管理**
   - 读取CPU调速器：`cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor 2>/dev/null`
   - 根据调速器设置确定电源模式

### 数据源
- `/sys/class/power_supply/` 电池和电源信息
- `/sys/devices/system/cpu/cpu0/cpufreq/` CPU频率调速器

### 使用的命令
- 使用`cat`读取sysfs条目
- 使用shell管道检测电源类型

### 错误处理
- 电池信息不可用时回退到"AC"电源
- 优雅处理缺失的sysfs条目
- 记录调试信息便于故障排除

### 检索到的信息
- 电源（交流/电池）
- 电池百分比
- 电池状态（充电/放电）
- 电源模式（性能/省电）

## 系统负载

### 方法
- `updateLoadAverage()`: 检索系统负载平均值

### 实现细节
1. **负载平均值**
   - 从`/proc/loadavg`读取前三个值
   - 分别表示1分钟、5分钟和15分钟的平均负载
   - 负载平均值表示运行队列中的平均进程数

### 数据源
- `/proc/loadavg` 系统负载平均值

### 使用的命令
- 文件I/O操作读取`/proc/loadavg`
- 无外部命令

### 错误处理
- 文件无法打开时记录错误
- 读取失败时将负载平均值初始化为0.0

### 检索到的信息
- 1分钟负载平均值
- 5分钟负载平均值
- 15分钟负载平均值

## 错误处理

所有方法都包含对以下情况的错误处理：
- 缺少文件或目录
- 权限问题
- 无效的数据格式
- 可能较慢的操作超时

## 依赖项

- Linux内核（推荐2.6.18+）
- 标准C++库
- POSIX库
- NetworkManager（用于WiFi SSID）
- RKNPU内核模块（用于NPU信息）
- Mali内核模块（用于GPU信息）

## 注意事项

1. 某些指标需要root权限才能完全使用功能
2. 硬件特定路径可能因不同的ARM SoC而异
3. 温度读数可能需要校准以提高准确性
4. 网络统计信息依赖于内核网络堆栈实现
5. 电源管理功能取决于硬件和内核支持
