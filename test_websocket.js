// WebSocket测试脚本
// 在浏览器控制台中运行此脚本来测试WebSocket连接

console.log("=== WebSocket摄像头测试开始 ===");

// 测试基本WebSocket连接
function testBasicWebSocket() {
    console.log("测试基本WebSocket连接: ws://192.168.124.12:8080/ws");
    
    const ws1 = new WebSocket('ws://192.168.124.12:8080/ws');
    
    ws1.onopen = function(event) {
        console.log("✅ 基本WebSocket连接成功");
        ws1.send("Hello from test!");
    };
    
    ws1.onmessage = function(event) {
        console.log("📨 基本WebSocket收到消息:", event.data);
        ws1.close();
    };
    
    ws1.onclose = function(event) {
        console.log("🔌 基本WebSocket连接已关闭");
        // 测试摄像头WebSocket
        setTimeout(testCameraWebSocket, 1000);
    };
    
    ws1.onerror = function(error) {
        console.error("❌ 基本WebSocket连接错误:", error);
    };
}

// 测试摄像头WebSocket连接
function testCameraWebSocket() {
    console.log("测试摄像头WebSocket连接: ws://192.168.124.12:8080/ws/camera");
    
    const ws2 = new WebSocket('ws://192.168.124.12:8080/ws/camera');
    ws2.binaryType = 'arraybuffer';
    
    ws2.onopen = function(event) {
        console.log("✅ 摄像头WebSocket连接成功");
        
        // 发送启动摄像头命令
        const command = JSON.stringify({
            action: 'start_camera',
            camera_id: 'default'
        });
        ws2.send(command);
        console.log("📤 发送启动摄像头命令:", command);
    };
    
    ws2.onmessage = function(event) {
        if (event.data instanceof ArrayBuffer) {
            console.log("📷 收到摄像头图像数据，大小:", event.data.byteLength, "字节");
        } else {
            console.log("📨 摄像头WebSocket收到文本消息:", event.data);
        }
    };
    
    ws2.onclose = function(event) {
        console.log("🔌 摄像头WebSocket连接已关闭，代码:", event.code, "原因:", event.reason);
    };
    
    ws2.onerror = function(error) {
        console.error("❌ 摄像头WebSocket连接错误:", error);
    };
    
    // 10秒后关闭连接
    setTimeout(() => {
        if (ws2.readyState === WebSocket.OPEN) {
            console.log("⏰ 10秒测试时间到，关闭连接");
            ws2.close();
        }
    }, 10000);
}

// 开始测试
testBasicWebSocket();

console.log("=== 测试脚本已启动，请查看控制台输出 ===");
console.log("如果需要手动测试，可以运行:");
console.log("testBasicWebSocket() - 测试基本WebSocket");
console.log("testCameraWebSocket() - 测试摄像头WebSocket");
