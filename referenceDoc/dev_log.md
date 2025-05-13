# 开发日志

## 2025-05-13 - 修复 ffmpeg-next 相关编译错误

### 问题描述

在编译 `camera-core` 模块时，遇到了与 ffmpeg-next 相关的两个编译错误：

1. 在 `video.rs` 的第 202 行：`Ok(encoder) => encoder.into(),` - 错误是 `ffmpeg_next::encoder::Encoder` 没有实现 `From<ffmpeg_next::encoder::Video>` trait。

2. 在 `video.rs` 的第 305 行：`if let Err(e) = context.output_context.write_packet(&packet) {` - 错误是 `ffmpeg_next::format::context::Output` 没有 `write_packet` 方法。

### 解决方案

1. 修改 FFmpegContext 结构体中的 encoder 字段类型：
   - 从 `ffmpeg::codec::encoder::Encoder` 改为 `ffmpeg::codec::encoder::Video`，以匹配 `open_as` 方法返回的类型。
   - 修改了 `video.rs` 第 50 行的结构体定义。

2. 使用底层的 FFI 函数来写入数据包：
   - 由于 ffmpeg-next 6.0.0 中没有直接的方法来写入数据包到输出上下文，使用了底层的 FFI 函数 `av_interleaved_write_frame`。
   - 修改了 `video.rs` 第 303-307 行的代码。

3. 修复借用问题：
   - 将 `ffmpeg_frame.data_mut(0)` 的调用移到了 `ffmpeg_frame.stride(0)` 之后，以避免同时对 `ffmpeg_frame` 进行可变和不可变借用。
   - 修改了 `video.rs` 第 247-251 行的代码。

4. 导入必要的 trait：
   - 导入了 `ffmpeg_next::codec::packet::Mut as PacketMut` trait，以便使用 `as_mut_ptr` 方法。
   - 在 `video.rs` 第 11 行添加了导入语句。

### 修改详情

#### 1. 修改 FFmpegContext 结构体

```rust
// 修改前
struct FFmpegContext {
    /// 输出格式上下文
    output_context: ffmpeg::format::context::Output,

    /// 视频流索引
    video_stream_index: usize,

    /// 编码器
    encoder: ffmpeg::codec::encoder::Encoder,

    /// 帧计数器
    frame_count: usize,
}

// 修改后
struct FFmpegContext {
    /// 输出格式上下文
    output_context: ffmpeg::format::context::Output,

    /// 视频流索引
    video_stream_index: usize,

    /// 编码器
    encoder: ffmpeg::codec::encoder::Video,

    /// 帧计数器
    frame_count: usize,
}
```

#### 2. 修改打开编码器的代码

```rust
// 修改前
// 打开编码器
let opened_encoder = match encoder.open_as(codec) {
    Ok(encoder) => encoder.into(),
    Err(e) => {
        return Err(Error::FFmpeg(format!("打开编码器失败: {}", e)));
    }
};

// 修改后
// 打开编码器
let opened_encoder = match encoder.open_as(codec) {
    Ok(encoder) => encoder,
    Err(e) => {
        return Err(Error::FFmpeg(format!("打开编码器失败: {}", e)));
    }
};
```

#### 3. 修改写入数据包的代码

```rust
// 修改前
// 写入包
// 使用 write_packet 方法
if let Err(e) = context.output_context.write_packet(&packet) {
    return Err(Error::FFmpeg(format!("写入帧失败: {}", e)));
}

// 修改后
// 写入包
// 使用底层 FFI 函数
unsafe {
    let result = ffmpeg::ffi::av_interleaved_write_frame(
        context.output_context.as_mut_ptr(),
        packet.as_mut_ptr(),
    );
    if result < 0 {
        return Err(Error::FFmpeg(format!("写入帧失败: {}", result)));
    }
}
```

#### 4. 修复借用问题

```rust
// 修改前
let dest = ffmpeg_frame.data_mut(0);
let dest_stride = ffmpeg_frame.stride(0) as usize;
let copy_len = std::cmp::min(stride, dest_stride);
dest[i * dest_stride..i * dest_stride + copy_len].copy_from_slice(&line[..copy_len]);

// 修改后
let dest_stride = ffmpeg_frame.stride(0) as usize;
let copy_len = std::cmp::min(stride, dest_stride);
let dest = ffmpeg_frame.data_mut(0);
dest[i * dest_stride..i * dest_stride + copy_len].copy_from_slice(&line[..copy_len]);
```

#### 5. 导入必要的 trait

```rust
// 添加导入
use ffmpeg_next::codec::packet::Mut as PacketMut;
```

### 环境信息

- 操作系统：Linux (Orange Pi 5 Plus)
- CPU 架构：ARM64 (RK3588)
- Rust 版本：Rust 2021 edition
- ffmpeg-next 版本：6.0.0

### 备注

这些修改使得代码能够正确地使用 ffmpeg-next 6.0.0 的 API，并成功编译。修复了视频录制功能中的编码器和数据包写入问题。

## 2025-05-14 - FFmpeg 集成计划

### 当前状态
- 项目目前使用 nokhwa 库进行摄像头捕获
- FFmpeg 集成（ffmpeg-next）已被注释掉，原因是与系统 FFmpeg 版本不兼容
- 系统上已安装 FFmpeg 5.1.3-4，并启用了 RK3588 的 MPP 和 RGA 支持
- 视频录制和拆分功能尚未实现

### 计划
1. 更新 Cargo.toml 文件，启用 ffmpeg-next 依赖
2. 实现 VideoRecorder 类，使用 FFmpeg 进行视频录制
3. 实现 VideoSplitter 类，使用 FFmpeg 进行视频拆分
4. 测试 RK3588 硬件加速功能

### 开发环境
- **开发板**: RK3588（香橙派 5 Plus）
- **CPU架构**: ARM64
- **操作系统**: Armbian（基于 Debian/Ubuntu 的 RK3588 优化版本）
- **FFmpeg版本**: 5.1.3-4，启用了 RK3588 的 MPP 和 RGA 支持

## 2025-05-14 - FFmpeg 集成实现

### 完成工作
1. 更新了 Cargo.toml 文件，启用了 ffmpeg-next 依赖
2. 实现了 VideoRecorder 类，使用 FFmpeg 进行视频录制
   - 支持 RK3588 硬件加速（h264_rkmpp 编码器）
   - 实现了帧转换和编码功能
   - 添加了错误处理和日志记录
3. 实现了 VideoSplitter 类，使用 FFmpeg 进行视频拆分
   - 支持异步拆分任务
   - 实现了进度跟踪和取消功能
   - 支持不同的图像格式和质量设置
4. 更新了 RecordingConfig 结构体，添加了宽度、高度和帧率字段

### 下一步计划
1. 测试 VideoRecorder 和 VideoSplitter 类的功能
2. 优化 RK3588 硬件加速的使用
3. 实现视频录制和拆分的 API 接口
4. 更新 Web 客户端界面，支持视频录制和拆分功能
