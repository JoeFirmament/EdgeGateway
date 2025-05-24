# RK3588 4GB内存优化实施指南

## 📋 概述

本指南提供了将RK3588双摄像头推流系统从7.7GB内存配置优化到4GB配置的详细实施方案。

**目标**: 在保持核心功能稳定的前提下，通过内存优化实现成本控制。

## 🎯 优化目标

### 内存使用目标
- **系统基础**: ≤2.3GB (从2.8GB优化)
- **应用程序**: ≤200MB (双摄像头推流)
- **缓存预留**: ≥500MB (系统缓存)
- **安全余量**: ≥900MB (应对峰值使用)

### 性能目标
- **双摄像头640x480@30fps**: 稳定运行
- **CPU使用率**: ≤60%
- **内存使用率**: ≤75%
- **系统响应**: 延迟增加≤10%

## 🔧 系统级优化

### 1. 内存压缩 (ZRAM)

#### 配置ZRAM
```bash
#!/bin/bash
# setup_zram.sh - ZRAM内存压缩配置

# 重置ZRAM设备
echo 1 > /sys/block/zram0/reset

# 设置压缩算法 (lz4性能最佳)
echo lz4 > /sys/block/zram0/comp_algorithm

# 设置ZRAM大小 (1GB)
echo 1G > /sys/block/zram0/disksize

# 创建交换分区
mkswap /dev/zram0

# 启用交换分区
swapon /dev/zram0 -p 10

echo "ZRAM配置完成"
```

#### 开机自动启用
```bash
# 添加到 /etc/rc.local
/path/to/setup_zram.sh
```

### 2. 内存管理优化

#### 调整内存回收策略
```bash
#!/bin/bash
# memory_tuning.sh - 内存管理优化

# 调整交换倾向 (降低swap使用)
echo 60 > /proc/sys/vm/swappiness

# 调整脏页回写
echo 15 > /proc/sys/vm/dirty_ratio
echo 5 > /proc/sys/vm/dirty_background_ratio

# 调整内存过量分配
echo 1 > /proc/sys/vm/overcommit_memory

# 清理页面缓存
echo 1 > /proc/sys/vm/drop_caches

echo "内存管理优化完成"
```

### 3. 系统服务精简

#### 禁用不必要的服务
```bash
#!/bin/bash
# disable_services.sh - 禁用不必要的系统服务

# 禁用蓝牙服务
systemctl disable bluetooth
systemctl stop bluetooth

# 禁用打印服务
systemctl disable cups
systemctl stop cups

# 禁用Avahi服务发现
systemctl disable avahi-daemon
systemctl stop avahi-daemon

# 禁用ModemManager
systemctl disable ModemManager
systemctl stop ModemManager

# 禁用NetworkManager (如果使用静态IP)
# systemctl disable NetworkManager

echo "系统服务精简完成"
```

#### 优化启动服务
```bash
# 查看启动时间
systemd-analyze blame

# 禁用慢启动服务
systemctl disable slow-service-name
```

## 💻 应用程序级优化

### 1. 内存池管理

#### 实现高效内存池
```cpp
// optimized_memory_pool.h
#pragma once
#include <vector>
#include <queue>
#include <mutex>
#include <memory>

class OptimizedMemoryPool {
private:
    struct MemoryBlock {
        std::unique_ptr<uint8_t[]> data;
        size_t size;
        bool in_use;
        
        MemoryBlock(size_t s) : size(s), in_use(false) {
            data = std::make_unique<uint8_t[]>(s);
        }
    };
    
    std::vector<std::unique_ptr<MemoryBlock>> blocks_;
    std::queue<MemoryBlock*> available_;
    std::mutex mutex_;
    size_t block_size_;
    size_t max_blocks_;

public:
    OptimizedMemoryPool(size_t block_size, size_t max_blocks) 
        : block_size_(block_size), max_blocks_(max_blocks) {
        // 预分配内存块
        for (size_t i = 0; i < max_blocks_; ++i) {
            auto block = std::make_unique<MemoryBlock>(block_size_);
            available_.push(block.get());
            blocks_.push_back(std::move(block));
        }
    }
    
    uint8_t* acquire() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (available_.empty()) {
            return nullptr; // 内存池耗尽
        }
        
        auto* block = available_.front();
        available_.pop();
        block->in_use = true;
        return block->data.get();
    }
    
    void release(uint8_t* ptr) {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& block : blocks_) {
            if (block->data.get() == ptr) {
                block->in_use = false;
                available_.push(block.get());
                break;
            }
        }
    }
    
    size_t getAvailableBlocks() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return available_.size();
    }
};
```

### 2. 零拷贝优化

#### 优化帧传输
```cpp
// zero_copy_frame.h
#pragma once
#include "camera/frame.h"

class ZeroCopyFrame {
private:
    const uint8_t* data_;
    size_t size_;
    bool owns_data_;

public:
    // 构造函数 - 不拷贝数据
    ZeroCopyFrame(const uint8_t* data, size_t size, bool owns = false)
        : data_(data), size_(size), owns_data_(owns) {}
    
    // 移动构造函数
    ZeroCopyFrame(ZeroCopyFrame&& other) noexcept
        : data_(other.data_), size_(other.size_), owns_data_(other.owns_data_) {
        other.data_ = nullptr;
        other.owns_data_ = false;
    }
    
    // 禁用拷贝构造函数
    ZeroCopyFrame(const ZeroCopyFrame&) = delete;
    ZeroCopyFrame& operator=(const ZeroCopyFrame&) = delete;
    
    ~ZeroCopyFrame() {
        if (owns_data_ && data_) {
            delete[] data_;
        }
    }
    
    const uint8_t* data() const { return data_; }
    size_t size() const { return size_; }
    
    // 直接发送到WebSocket
    void sendToWebSocket(crow::websocket::connection& conn) {
        conn.send_binary(std::string_view(
            reinterpret_cast<const char*>(data_), size_));
    }
};
```

### 3. 缓冲区优化

#### 减少帧缓冲区
```cpp
// optimized_camera_config.h
#pragma once

namespace optimized_config {
    // 4GB内存优化配置
    constexpr int FRAME_BUFFER_COUNT = 3;        // 从5减少到3
    constexpr size_t WEBSOCKET_BUFFER_SIZE = 1024 * 1024;  // 1MB
    constexpr size_t MAX_FRAME_SIZE = 640 * 480 * 3;       // 最大帧大小
    constexpr int MAX_CONCURRENT_CLIENTS = 4;              // 限制并发客户端
    
    // 内存池配置
    constexpr size_t MEMORY_POOL_BLOCK_SIZE = MAX_FRAME_SIZE;
    constexpr size_t MEMORY_POOL_MAX_BLOCKS = FRAME_BUFFER_COUNT * 2;
}
```

## 📊 监控和调试

### 1. 内存监控脚本

#### 实时内存监控
```bash
#!/bin/bash
# memory_monitor.sh - 内存使用监控

LOG_FILE="/var/log/memory_usage.log"

while true; do
    TIMESTAMP=$(date '+%Y-%m-%d %H:%M:%S')
    
    # 获取内存信息
    MEMORY_INFO=$(free -h | grep Mem)
    SWAP_INFO=$(free -h | grep Swap)
    
    # 获取进程内存使用
    CAMERA_PROCESS=$(ps aux | grep dual_camera | grep -v grep | awk '{print $4}')
    
    # 记录日志
    echo "[$TIMESTAMP] Memory: $MEMORY_INFO" >> $LOG_FILE
    echo "[$TIMESTAMP] Swap: $SWAP_INFO" >> $LOG_FILE
    echo "[$TIMESTAMP] Camera Process: ${CAMERA_PROCESS:-0}%" >> $LOG_FILE
    echo "[$TIMESTAMP] ---" >> $LOG_FILE
    
    sleep 60  # 每分钟记录一次
done
```

### 2. 性能分析工具

#### 内存泄漏检测
```bash
#!/bin/bash
# memory_leak_check.sh - 内存泄漏检测

# 使用valgrind检测内存泄漏
valgrind --tool=memcheck \
         --leak-check=full \
         --show-leak-kinds=all \
         --track-origins=yes \
         --log-file=valgrind_output.log \
         ./dual_camera_websocket_server

echo "内存泄漏检测完成，查看 valgrind_output.log"
```

#### 性能分析
```bash
#!/bin/bash
# performance_analysis.sh - 性能分析

# CPU和内存使用分析
top -b -n 1 -p $(pgrep dual_camera) > performance.log

# 系统调用分析
strace -c -p $(pgrep dual_camera) 2>&1 | tee syscall_analysis.log

# 内存映射分析
cat /proc/$(pgrep dual_camera)/smaps > memory_mapping.log

echo "性能分析完成"
```

## 🧪 测试验证

### 1. 4GB环境模拟测试

#### 限制内存使用
```bash
#!/bin/bash
# simulate_4gb.sh - 模拟4GB内存环境

# 创建cgroup限制内存
sudo cgcreate -g memory:/4gb_test
echo 4G | sudo tee /sys/fs/cgroup/memory/4gb_test/memory.limit_in_bytes

# 在限制环境中运行程序
sudo cgexec -g memory:4gb_test ./dual_camera_websocket_server

echo "4GB环境模拟测试启动"
```

### 2. 压力测试脚本

#### 多客户端连接测试
```bash
#!/bin/bash
# stress_test.sh - 压力测试

SERVER_URL="ws://localhost:8081/ws/video"
CLIENT_COUNT=4

# 启动多个客户端连接
for i in $(seq 1 $CLIENT_COUNT); do
    echo "启动客户端 $i"
    node websocket_client.js $SERVER_URL &
    sleep 2
done

echo "压力测试启动，$CLIENT_COUNT 个客户端连接"
```

### 3. 稳定性测试

#### 长时间运行测试
```bash
#!/bin/bash
# stability_test.sh - 24小时稳定性测试

TEST_DURATION=86400  # 24小时 (秒)
START_TIME=$(date +%s)

echo "开始24小时稳定性测试"

while true; do
    CURRENT_TIME=$(date +%s)
    ELAPSED=$((CURRENT_TIME - START_TIME))
    
    if [ $ELAPSED -ge $TEST_DURATION ]; then
        echo "24小时测试完成"
        break
    fi
    
    # 检查进程是否还在运行
    if ! pgrep dual_camera > /dev/null; then
        echo "进程异常退出，测试失败"
        exit 1
    fi
    
    # 检查内存使用
    MEMORY_USAGE=$(free | grep Mem | awk '{printf "%.1f", $3/$2 * 100.0}')
    echo "运行时间: ${ELAPSED}s, 内存使用: ${MEMORY_USAGE}%"
    
    sleep 300  # 每5分钟检查一次
done
```

## 📈 部署建议

### 1. 分阶段部署策略

#### 阶段1: 开发环境优化 (7.7GB)
- 实施应用程序级优化
- 验证内存池和零拷贝技术
- 建立性能基准

#### 阶段2: 模拟环境测试 (7.7GB限制到4GB)
- 使用cgroup限制内存使用
- 运行完整功能测试
- 验证优化效果

#### 阶段3: 实际硬件验证 (4GB)
- 在真实4GB硬件上测试
- 长期稳定性验证
- 性能回归测试

#### 阶段4: 生产部署决策
- 基于测试结果做最终决策
- 制定回滚计划
- 建立监控和告警

### 2. 监控和告警

#### 内存使用告警
```bash
# 添加到crontab
*/5 * * * * /path/to/memory_alert.sh

# memory_alert.sh
MEMORY_USAGE=$(free | grep Mem | awk '{printf "%.0f", $3/$2 * 100.0}')
if [ $MEMORY_USAGE -gt 85 ]; then
    echo "警告: 内存使用率达到 ${MEMORY_USAGE}%" | mail -s "内存告警" admin@example.com
fi
```

## 🎯 成功标准

### 功能标准
- ✅ 双摄像头640x480@30fps稳定推流
- ✅ 支持4个并发客户端连接
- ✅ 24小时连续运行无异常
- ✅ 内存使用率≤75%

### 性能标准
- ✅ 系统响应延迟增加≤10%
- ✅ CPU使用率≤60%
- ✅ 无内存泄漏
- ✅ 启动时间≤30秒

### 稳定性标准
- ✅ 7天连续运行测试通过
- ✅ 压力测试通过
- ✅ 异常恢复测试通过
- ✅ 内存碎片化控制在合理范围

## 📋 检查清单

### 优化实施检查
- [ ] ZRAM内存压缩配置
- [ ] 系统服务精简
- [ ] 内存管理参数调优
- [ ] 应用程序内存池实现
- [ ] 零拷贝技术应用
- [ ] 缓冲区大小优化

### 测试验证检查
- [ ] 4GB环境模拟测试
- [ ] 多客户端压力测试
- [ ] 24小时稳定性测试
- [ ] 内存泄漏检测
- [ ] 性能回归测试
- [ ] 实际硬件验证

### 部署准备检查
- [ ] 监控脚本部署
- [ ] 告警机制配置
- [ ] 回滚方案准备
- [ ] 文档更新完成
- [ ] 团队培训完成

通过遵循本指南，可以安全有效地将RK3588双摄像头推流系统优化到4GB内存配置，在降低成本的同时保持系统的稳定性和性能。
