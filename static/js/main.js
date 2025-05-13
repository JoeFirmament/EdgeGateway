document.addEventListener('DOMContentLoaded', function() {
    // 获取API状态
    function getApiStatus() {
        fetch('/api')
        .then(response => response.json())
        .then(data => {
            if (data.status === 'success') {
                console.log('API服务器正常运行，版本:', data.version);
                
                // 可以在这里添加一些UI更新，例如显示API状态
                const statusElement = document.getElementById('api-status');
                if (statusElement) {
                    statusElement.textContent = '服务器状态: 正常运行';
                    statusElement.className = 'status-ok';
                }
            }
        })
        .catch(error => {
            console.error('API请求失败:', error);
            
            // 更新UI显示API不可用
            const statusElement = document.getElementById('api-status');
            if (statusElement) {
                statusElement.textContent = '服务器状态: 不可用';
                statusElement.className = 'status-error';
            }
        });
    }
    
    // 获取摄像头状态
    function getCameraStatus() {
        fetch('/api/camera/info')
        .then(response => response.json())
        .then(data => {
            if (data.status === 'success') {
                // 更新摄像头状态UI
                const cameraStatusElement = document.getElementById('camera-status');
                if (cameraStatusElement) {
                    if (data.camera.open) {
                        cameraStatusElement.textContent = '摄像头状态: 已打开';
                        cameraStatusElement.className = 'status-ok';
                    } else {
                        cameraStatusElement.textContent = '摄像头状态: 已关闭';
                        cameraStatusElement.className = 'status-warning';
                    }
                }
                
                // 更新摄像头信息
                const cameraInfoElement = document.getElementById('camera-info');
                if (cameraInfoElement && data.params) {
                    cameraInfoElement.innerHTML = `
                        <p>设备: ${data.camera.device || '未知'}</p>
                        <p>分辨率: ${data.params.width}x${data.params.height}</p>
                        <p>帧率: ${data.params.fps} FPS</p>
                        <p>格式: ${data.params.format}</p>
                    `;
                }
            }
        })
        .catch(error => {
            console.error('获取摄像头信息失败:', error);
            
            // 更新UI显示摄像头不可用
            const cameraStatusElement = document.getElementById('camera-status');
            if (cameraStatusElement) {
                cameraStatusElement.textContent = '摄像头状态: 不可用';
                cameraStatusElement.className = 'status-error';
            }
        });
    }
    
    // 获取系统状态摘要
    function getSystemSummary() {
        fetch('/api/system/info')
        .then(response => response.json())
        .then(data => {
            if (data.status === 'success') {
                // 更新系统状态UI
                const cpuUsageElement = document.getElementById('cpu-usage-summary');
                if (cpuUsageElement) {
                    cpuUsageElement.textContent = `CPU使用率: ${data.cpu.usage.toFixed(1)}%`;
                    
                    // 根据使用率设置颜色
                    if (data.cpu.usage < 60) {
                        cpuUsageElement.className = 'status-ok';
                    } else if (data.cpu.usage < 85) {
                        cpuUsageElement.className = 'status-warning';
                    } else {
                        cpuUsageElement.className = 'status-error';
                    }
                }
                
                const memoryUsageElement = document.getElementById('memory-usage-summary');
                if (memoryUsageElement) {
                    memoryUsageElement.textContent = `内存使用率: ${data.memory.usage.toFixed(1)}%`;
                    
                    // 根据使用率设置颜色
                    if (data.memory.usage < 70) {
                        memoryUsageElement.className = 'status-ok';
                    } else if (data.memory.usage < 90) {
                        memoryUsageElement.className = 'status-warning';
                    } else {
                        memoryUsageElement.className = 'status-error';
                    }
                }
                
                // 更新存储使用率
                if (data.storage && data.storage.length > 0) {
                    const storageUsageElement = document.getElementById('storage-usage-summary');
                    if (storageUsageElement) {
                        // 使用根分区（通常是第一个）
                        const rootStorage = data.storage[0];
                        storageUsageElement.textContent = `存储使用率: ${rootStorage.usage.toFixed(1)}%`;
                        
                        // 根据使用率设置颜色
                        if (rootStorage.usage < 75) {
                            storageUsageElement.className = 'status-ok';
                        } else if (rootStorage.usage < 90) {
                            storageUsageElement.className = 'status-warning';
                        } else {
                            storageUsageElement.className = 'status-error';
                        }
                    }
                }
                
                // 更新系统运行时间
                const uptimeElement = document.getElementById('system-uptime');
                if (uptimeElement && data.system.uptime) {
                    uptimeElement.textContent = `运行时间: ${data.system.uptime}`;
                }
            }
        })
        .catch(error => {
            console.error('获取系统信息失败:', error);
        });
    }
    
    // 初始化：获取各种状态
    getApiStatus();
    getCameraStatus();
    getSystemSummary();
    
    // 定期更新状态
    setInterval(getApiStatus, 30000);  // 每30秒更新一次API状态
    setInterval(getCameraStatus, 10000);  // 每10秒更新一次摄像头状态
    setInterval(getSystemSummary, 5000);  // 每5秒更新一次系统状态摘要
});
