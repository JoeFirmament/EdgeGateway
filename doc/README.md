# 摄像头服务器项目

## 项目状态
[!WARNING]  
当前版本存在Segmentation fault问题，正在调试中

## 已知问题
- 在高并发情况下可能出现段错误
- 正在优化MJPEG流传输的线程安全

## 最新进展
- 已添加详细的调试日志
- 优化了内存管理机制
- 改进了多线程锁机制

## 构建说明
```bash
mkdir build && cd build
cmake ..
make
```

## 调试建议
1. 使用gdb分析核心转储：
```bash
gdb ./cam_server core
```
2. 查看详细日志：
```bash
tail -f build_module_test/bin/logs/cam_server.log
```
