/**
 * WebSocket视频流修复脚本
 * 
 * 使用方法：
 * 1. 在浏览器中打开视频流页面
 * 2. 按F12打开开发者工具，切换到Console标签页
 * 3. 连接WebSocket后，复制粘贴此脚本并运行
 * 4. 启动摄像头即可看到视频流
 * 
 * 问题：原始WebSocket消息处理器逻辑有缺陷，导致handleVideoFrame函数未被调用
 * 解决：重新定义WebSocket消息处理器和handleVideoFrame函数
 */

console.log('🔧 开始应用WebSocket视频流修复...');

// 检查必要的变量是否存在
if (typeof ws === 'undefined') {
    console.log('❌ WebSocket对象(ws)不存在，请先连接WebSocket');
} else if (ws.readyState !== WebSocket.OPEN) {
    console.log('❌ WebSocket未连接，当前状态:', ws.readyState);
} else {
    console.log('✅ WebSocket连接正常');
}

if (typeof videoCanvas === 'undefined') {
    console.log('❌ videoCanvas元素不存在');
} else {
    console.log('✅ videoCanvas元素存在');
}

if (typeof ctx === 'undefined') {
    console.log('❌ Canvas context不存在');
} else {
    console.log('✅ Canvas context存在');
}

// 步骤1：重新定义WebSocket消息处理器
if (ws && ws.readyState === WebSocket.OPEN) {
    ws.onmessage = function(event) {
        // 处理ArrayBuffer类型的二进制数据（视频帧）
        if (event.data instanceof ArrayBuffer) {
            console.log('📦 收到ArrayBuffer，大小:', event.data.byteLength, '字节');
            handleVideoFrame(event.data);
        } 
        // 处理Blob类型的二进制数据（备用方案）
        else if (event.data instanceof Blob) {
            console.log('📦 收到Blob，大小:', event.data.size, '字节，转换为ArrayBuffer');
            event.data.arrayBuffer().then(function(arrayBuffer) {
                handleVideoFrame(arrayBuffer);
            });
        } 
        // 处理文本消息（命令响应等）
        else if (typeof event.data === 'string') {
            try {
                const message = JSON.parse(event.data);
                console.log('📝 收到文本消息:', message.type || 'unknown');
                // 调用原始的文本消息处理函数
                if (typeof handleTextMessage === 'function') {
                    handleTextMessage(event.data);
                }
            } catch (e) {
                console.log('📝 收到非JSON文本:', event.data.substring(0, 50));
            }
        } 
        // 未知类型
        else {
            console.log('❓ 未知消息类型:', typeof event.data, event.data.constructor.name);
        }
    };
    console.log('✅ WebSocket消息处理器已重新定义');
}

// 步骤2：重新定义handleVideoFrame函数
window.handleVideoFrame = function(arrayBuffer) {
    // 更新帧计数
    if (typeof receivedFrames !== 'undefined') {
        receivedFrames++;
        if (typeof frameCount !== 'undefined' && frameCount.textContent !== undefined) {
            frameCount.textContent = receivedFrames;
        }
    }
    
    try {
        // 将ArrayBuffer转换为Blob
        const blob = new Blob([arrayBuffer], { type: 'image/jpeg' });
        
        // 创建对象URL
        const url = URL.createObjectURL(blob);
        
        // 创建图像对象
        const img = new Image();
        
        // 图像加载成功时的处理
        img.onload = function() {
            try {
                // 清空Canvas并绘制新图像
                ctx.clearRect(0, 0, videoCanvas.width, videoCanvas.height);
                ctx.drawImage(img, 0, 0, videoCanvas.width, videoCanvas.height);
                
                // 释放对象URL
                URL.revokeObjectURL(url);
                
                // 只在前几帧输出成功信息，避免日志过多
                if (receivedFrames <= 3 || receivedFrames % 100 === 1) {
                    console.log('✅ 图像绘制成功，尺寸:', img.width + 'x' + img.height, '帧号:', receivedFrames);
                }
            } catch (drawError) {
                console.log('❌ Canvas绘制失败:', drawError.message);
                URL.revokeObjectURL(url);
            }
        };
        
        // 图像加载失败时的处理
        img.onerror = function() {
            console.log('❌ 图像加载失败，可能是JPEG数据损坏');
            URL.revokeObjectURL(url);
        };
        
        // 开始加载图像
        img.src = url;
        
    } catch (e) {
        console.log('❌ handleVideoFrame异常:', e.message);
    }
};

console.log('✅ handleVideoFrame函数已重新定义');

// 步骤3：验证修复是否成功
console.log('🎯 修复完成！现在可以启动摄像头测试视频流');
console.log('📋 如果仍有问题，请检查：');
console.log('   1. 后端服务器是否正常运行');
console.log('   2. 摄像头是否正确启动');
console.log('   3. 浏览器控制台是否有其他错误信息');

// 可选：添加性能监控
let lastFrameTime = Date.now();
let frameRateCounter = 0;

const originalHandleVideoFrame = window.handleVideoFrame;
window.handleVideoFrame = function(arrayBuffer) {
    // 计算帧率
    frameRateCounter++;
    const now = Date.now();
    if (now - lastFrameTime >= 1000) { // 每秒统计一次
        if (frameRateCounter > 1) { // 避免首次统计
            console.log('📊 当前帧率:', frameRateCounter, 'FPS，帧大小:', Math.round(arrayBuffer.byteLength/1024), 'KB');
        }
        frameRateCounter = 0;
        lastFrameTime = now;
    }
    
    // 调用原始处理函数
    originalHandleVideoFrame.call(this, arrayBuffer);
};

console.log('✅ 已添加帧率监控');
console.log('🚀 WebSocket视频流修复脚本加载完成！');
