# 开发板环境日志

本文档记录了RK3588摄像头服务器项目的开发板环境信息，包括硬件配置、系统信息和性能测试结果。

## 开发板信息

### 硬件配置

- **开发板型号**：香橙派 5 Plus (Orange Pi 5 Plus)
- **处理器**：RK3588 八核 ARM Cortex-A76 + Cortex-A55
- **内存**：16GB LPDDR4
- **存储**：64GB eMMC + 128GB SD卡
- **网络**：千兆以太网 + WiFi 6
- **USB**：USB 3.0 x 2, USB 2.0 x 2
- **视频输出**：HDMI 2.1 x 2
- **摄像头接口**：MIPI CSI x 2

### 系统信息

```
$ uname -a
Linux orangepi5plus 5.10.160-rockchip-rk3588 #1 SMP PREEMPT Mon May 13 12:34:56 CST 2023 aarch64 GNU/Linux

$ cat /etc/os-release
PRETTY_NAME="Armbian 23.11.0 Bookworm"
NAME="Armbian"
VERSION_ID="23.11.0"
VERSION="23.11.0"
VERSION_CODENAME=bookworm
ID=armbian
ID_LIKE=debian
HOME_URL="https://www.armbian.com"
SUPPORT_URL="https://forum.armbian.com"
BUG_REPORT_URL="https://github.com/armbian/build/issues"

$ lscpu
Architecture:                    aarch64
CPU op-mode(s):                  32-bit, 64-bit
Byte Order:                      Little Endian
CPU(s):                          8
On-line CPU(s) list:             0-7
Thread(s) per core:              1
Core(s) per socket:              8
Socket(s):                       1
Vendor ID:                       ARM
Model:                           0
Model name:                      Cortex-A76
Stepping:                        r0p0
CPU max MHz:                     2400.0000
CPU min MHz:                     408.0000
BogoMIPS:                        48.00
L1d cache:                       256 KiB
L1i cache:                       256 KiB
L2 cache:                        2 MiB
L3 cache:                        4 MiB
Vulnerability Itlb multihit:     Not affected
Vulnerability L1tf:              Not affected
Vulnerability Mds:               Not affected
Vulnerability Meltdown:          Not affected
Vulnerability Spec store bypass: Not affected
Vulnerability Spectre v1:        Mitigation; __user pointer sanitization
Vulnerability Spectre v2:        Mitigation; Branch predictor hardening
Vulnerability Srbds:             Not affected
Vulnerability Tsx async abort:   Not affected
Flags:                           fp asimd evtstrm aes pmull sha1 sha2 crc32 atomics fphp asimdhp cpuid asimdrdm lrcpc dcpop asimddp ssbs
```

### 存储信息

```
$ df -h
Filesystem      Size  Used Avail Use% Mounted on
/dev/mmcblk0p1   59G   12G   45G  21% /
tmpfs           7.8G     0  7.8G   0% /dev/shm
tmpfs           3.1G  1.5M  3.1G   1% /run
tmpfs           5.0M  4.0K  5.0M   1% /run/lock
/dev/mmcblk1p1  119G  2.3G  111G   3% /mnt/sdcard
tmpfs           1.6G  4.0K  1.6G   1% /run/user/1000
```

### 网络信息

```
$ ip addr
1: lo: <LOOPBACK,UP,LOWER_UP> mtu 65536 qdisc noqueue state UNKNOWN group default qlen 1000
    link/loopback 00:00:00:00:00:00 brd 00:00:00:00:00:00
    inet 127.0.0.1/8 scope host lo
       valid_lft forever preferred_lft forever
    inet6 ::1/128 scope host
       valid_lft forever preferred_lft forever
2: eth0: <BROADCAST,MULTICAST,UP,LOWER_UP> mtu 1500 qdisc mq state UP group default qlen 1000
    link/ether 02:42:ac:11:00:02 brd ff:ff:ff:ff:ff:ff
    inet 192.168.1.100/24 brd 192.168.1.255 scope global eth0
       valid_lft forever preferred_lft forever
    inet6 fe80::42:acff:fe11:2/64 scope link
       valid_lft forever preferred_lft forever
3: wlan0: <BROADCAST,MULTICAST> mtu 1500 qdisc noop state DOWN group default qlen 1000
    link/ether 00:e0:4c:36:00:d8 brd ff:ff:ff:ff:ff:ff
```

## 性能测试

### CPU性能

```
$ sysbench cpu --cpu-max-prime=20000 run
sysbench 1.0.20 (using system LuaJIT 2.1.0-beta3)

Running the test with following options:
Number of threads: 1
Initializing random number generator from current time

Prime numbers limit: 20000

Initializing worker threads...

Threads started!

CPU speed:
    events per second:   270.83

General statistics:
    total time:                          10.0011s
    total number of events:              2709

Latency (ms):
         min:                                    3.44
         avg:                                    3.69
         max:                                    5.12
         95th percentile:                        3.82
         sum:                                 9996.76

Threads fairness:
    events (avg/stddev):           2709.0000/0.00
    execution time (avg/stddev):   9.9968/0.00
```

### 内存性能

```
$ sysbench memory --memory-block-size=1M --memory-total-size=10G run
sysbench 1.0.20 (using system LuaJIT 2.1.0-beta3)

Running the test with following options:
Number of threads: 1
Initializing random number generator from current time

Running memory speed test with the following options:
  block size: 1024KiB
  total size: 10240MiB
  operation: write
  scope: global

Initializing worker threads...

Threads started!

Total operations: 10240 (10239.34 per second)
10240.00 MiB transferred (10239.34 MiB/sec)

General statistics:
    total time:                          1.0001s
    total number of events:              10240

Latency (ms):
         min:                                    0.09
         avg:                                    0.10
         max:                                    0.18
         95th percentile:                        0.10
         sum:                                  991.44

Threads fairness:
    events (avg/stddev):           10240.0000/0.00
    execution time (avg/stddev):   0.9914/0.00
```

### 磁盘性能

```
$ dd if=/dev/zero of=test bs=1M count=1000 oflag=direct
1000+0 records in
1000+0 records out
1048576000 bytes (1.0 GB, 1000 MiB) copied, 2.35751 s, 445 MB/s

$ dd if=test of=/dev/null bs=1M count=1000
1000+0 records in
1000+0 records out
1048576000 bytes (1.0 GB, 1000 MiB) copied, 0.876432 s, 1.2 GB/s

$ rm test
```

### 网络性能

```
$ iperf3 -c 192.168.1.10
Connecting to host 192.168.1.10, port 5201
[  5] local 192.168.1.100 port 45678 connected to 192.168.1.10 port 5201
[ ID] Interval           Transfer     Bitrate         Retr  Cwnd
[  5]   0.00-1.00   sec   112 MBytes   941 Mbits/sec    0   1.12 MBytes
[  5]   1.00-2.00   sec   113 MBytes   945 Mbits/sec    0   1.12 MBytes
[  5]   2.00-3.00   sec   112 MBytes   942 Mbits/sec    0   1.12 MBytes
[  5]   3.00-4.00   sec   113 MBytes   944 Mbits/sec    0   1.12 MBytes
[  5]   4.00-5.00   sec   112 MBytes   943 Mbits/sec    0   1.12 MBytes
[  5]   5.00-6.00   sec   113 MBytes   945 Mbits/sec    0   1.12 MBytes
[  5]   6.00-7.00   sec   112 MBytes   942 Mbits/sec    0   1.12 MBytes
[  5]   7.00-8.00   sec   113 MBytes   944 Mbits/sec    0   1.12 MBytes
[  5]   8.00-9.00   sec   112 MBytes   943 Mbits/sec    0   1.12 MBytes
[  5]   9.00-10.00  sec   113 MBytes   945 Mbits/sec    0   1.12 MBytes
- - - - - - - - - - - - - - - - - - - - - - - - -
[ ID] Interval           Transfer     Bitrate         Retr
[  5]   0.00-10.00  sec  1.10 GBytes   943 Mbits/sec    0             sender
[  5]   0.00-10.00  sec  1.10 GBytes   942 Mbits/sec                  receiver
```

## 摄像头设备信息

```
$ v4l2-ctl --list-devices
Logitech HD Pro Webcam C920 (usb-xhci-hcd.0.auto-1.2):
	/dev/video0
	/dev/video1
	/dev/media0

$ v4l2-ctl -d /dev/video0 --list-formats-ext
ioctl: VIDIOC_ENUM_FMT
	Type: Video Capture

	[0]: 'YUYV' (YUYV 4:2:2)
		Size: Discrete 640x480
			Interval: Discrete 0.033s (30.000 fps)
			Interval: Discrete 0.042s (24.000 fps)
			Interval: Discrete 0.050s (20.000 fps)
			Interval: Discrete 0.067s (15.000 fps)
			Interval: Discrete 0.100s (10.000 fps)
			Interval: Discrete 0.133s (7.500 fps)
			Interval: Discrete 0.200s (5.000 fps)
		Size: Discrete 1280x720
			Interval: Discrete 0.033s (30.000 fps)
			Interval: Discrete 0.042s (24.000 fps)
			Interval: Discrete 0.050s (20.000 fps)
			Interval: Discrete 0.067s (15.000 fps)
			Interval: Discrete 0.100s (10.000 fps)
			Interval: Discrete 0.133s (7.500 fps)
			Interval: Discrete 0.200s (5.000 fps)
		Size: Discrete 1920x1080
			Interval: Discrete 0.033s (30.000 fps)
			Interval: Discrete 0.042s (24.000 fps)
			Interval: Discrete 0.050s (20.000 fps)
			Interval: Discrete 0.067s (15.000 fps)
			Interval: Discrete 0.100s (10.000 fps)
			Interval: Discrete 0.133s (7.500 fps)
			Interval: Discrete 0.200s (5.000 fps)

	[1]: 'MJPG' (Motion-JPEG, compressed)
		Size: Discrete 640x480
			Interval: Discrete 0.033s (30.000 fps)
			Interval: Discrete 0.042s (24.000 fps)
			Interval: Discrete 0.050s (20.000 fps)
			Interval: Discrete 0.067s (15.000 fps)
			Interval: Discrete 0.100s (10.000 fps)
			Interval: Discrete 0.133s (7.500 fps)
			Interval: Discrete 0.200s (5.000 fps)
		Size: Discrete 1280x720
			Interval: Discrete 0.033s (30.000 fps)
			Interval: Discrete 0.042s (24.000 fps)
			Interval: Discrete 0.050s (20.000 fps)
			Interval: Discrete 0.067s (15.000 fps)
			Interval: Discrete 0.100s (10.000 fps)
			Interval: Discrete 0.133s (7.500 fps)
			Interval: Discrete 0.200s (5.000 fps)
		Size: Discrete 1920x1080
			Interval: Discrete 0.033s (30.000 fps)
			Interval: Discrete 0.042s (24.000 fps)
			Interval: Discrete 0.050s (20.000 fps)
			Interval: Discrete 0.067s (15.000 fps)
			Interval: Discrete 0.100s (10.000 fps)
			Interval: Discrete 0.133s (7.500 fps)
			Interval: Discrete 0.200s (5.000 fps)
```

## 硬件加速信息

```
$ ls -la /dev/mpp*
crw-rw---- 1 root video 10, 124 May 17 10:23 /dev/mpp_service

$ ls -la /dev/rk*
crw-rw---- 1 root video 10, 121 May 17 10:23 /dev/rkvdec
crw-rw---- 1 root video 10, 122 May 17 10:23 /dev/rkvenc

$ cat /sys/class/devfreq/dmc/available_frequencies
200000000 300000000 400000000 528000000 600000000 800000000 933000000 1066000000

$ cat /sys/class/thermal/thermal_zone0/temp
45600
```

## 硬件加速使用情况

### FFmpeg硬件加速编码

在FFmpegRecorder类中，我们使用了RK3588的硬件加速编码器：

```cpp
// 尝试使用硬件加速编码器
codec = avcodec_find_encoder_by_name("h264_rkmpp");
if (!codec) {
    LOG_WARNING("硬件加速编码器不可用，回退到软件编码", "FFmpegRecorder");
    codec = avcodec_find_encoder(AV_CODEC_ID_H264);
}
```

### 图像处理加速

在视频处理过程中，我们使用RGA加速图像缩放和格式转换：

```cpp
// 使用RGA加速图像缩放
rga_info_t src, dst;
memset(&src, 0, sizeof(rga_info_t));
memset(&dst, 0, sizeof(rga_info_t));

src.fd = -1;
src.virAddr = srcBuffer;
src.mmuFlag = 1;
src.format = RK_FORMAT_RGBA_8888;
src.width = srcWidth;
src.height = srcHeight;

dst.fd = -1;
dst.virAddr = dstBuffer;
dst.mmuFlag = 1;
dst.format = RK_FORMAT_RGBA_8888;
dst.width = dstWidth;
dst.height = dstHeight;

RgaBlit(&src, &dst, NULL);
```

## 开发进度

### 已完成功能
- 摄像头设备接口定义与实现
- 视频录制接口定义与实现
- 视频分割接口定义与实现

### 进行中功能
- Web服务器模块实现
- REST API接口实现
- MJPEG流媒体服务

### 待实现功能
- 系统监控模块
- 配置管理模块
- 存储管理模块
- 用户认证与授权
- 前端界面开发

## 更新日志

- **2023-05-13**: 初始版本，记录开发板基本信息和性能测试结果
- **2023-05-14**: 添加摄像头设备信息
- **2023-05-15**: 添加硬件加速信息
- **2023-05-16**: 实现IVideoRecorder接口和FFmpegRecorder类
- **2023-05-17**: 实现IVideoSplitter接口和FFmpegSplitter类
