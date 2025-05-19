// 在文件开头添加全局变量
let selectedCamera = null;

document.addEventListener('DOMContentLoaded', function() {
    // 更新时钟显示
    function updateClock() {
        const now = new Date();
        const timeElement = document.getElementById('clock');
        if (timeElement) {
            timeElement.textContent = now.toLocaleTimeString('zh-CN', {
                hour: '2-digit',
                minute: '2-digit',
                second: '2-digit'
            });
        }
    }

    // 每秒更新一次时钟
    updateClock();
    setInterval(updateClock, 1000);

    // 更新API状态显示
    function updateApiStatus(isOnline) {
        const statusElement = document.getElementById('api-status');
        if (statusElement) {
            statusElement.textContent = isOnline ? '在线' : '离线';
            statusElement.className = isOnline ? 'status-ok' : 'status-error';
        }
    }

    // 更新摄像头状态显示
    function updateCameraStatus(data) {
        const statusElement = document.getElementById('camera-status');
        if (!statusElement) return;

        // 获取所有摄像头信息
        fetch('/api/camera/list')
        .then(response => response.json())
        .then(listData => {
            let statusHtml = '<div class="camera-status-container">';
            
            // 遍历所有摄像头
            listData.cameras.forEach(camera => {
                const isActive = data && data.device_info && data.device_info.path === camera.path;
                const statusClass = isActive ? (data.is_capturing ? 'status-ok' : 'status-warning') : 'status-offline';
                const statusText = isActive ? (data.is_capturing ? '正在预览' : '已打开') : '未使用';
                
                statusHtml += `
                    <div class="camera-info ${isActive ? 'active-camera-info' : ''}">
                        <div class="camera-info-header">
                            <h4>${camera.name}</h4>
                            <span class="device-path">${camera.path}</span>
                        </div>
                        <div class="camera-info-grid">
                            <div class="info-row">
                                <span class="info-key">设备状态:</span>
                                <span class="info-value ${statusClass}">${statusText}</span>
                            </div>
                            <div class="info-row">
                                <span class="info-key">支持格式:</span>
                                <span class="info-value">${Object.keys(camera.formats).join(', ')}</span>
                            </div>
                            <div class="info-row">
                                <span class="info-key">最大分辨率:</span>
                                <span class="info-value">${getMaxResolution(camera.formats)}</span>
                            </div>
                            ${isActive && data.params ? `
                                <div class="info-row">
                                    <span class="info-key">当前分辨率:</span>
                                    <span class="info-value highlight">${data.params.width}x${data.params.height}</span>
                                </div>
                                <div class="info-row">
                                    <span class="info-key">当前帧率:</span>
                                    <span class="info-value highlight">${data.params.fps} FPS</span>
                                </div>
                                <div class="info-row">
                                    <span class="info-key">当前格式:</span>
                                    <span class="info-value highlight">${data.params.format}</span>
                                </div>
                                <div class="info-row">
                                    <span class="info-key">工作模式:</span>
                                    <span class="info-value highlight">${data.is_capturing ? (data.params.format === 'MJPG' ? 'MJPEG流' : 'YUYV转换') : '未预览'}</span>
                                </div>
                                <div class="info-row">
                                    <span class="info-key">连接状态:</span>
                                    <span class="info-value ${data.is_capturing ? 'status-ok' : 'status-warning'}">${data.is_capturing ? '已连接' : '未连接'}</span>
                                </div>
                                <div class="info-row">
                                    <span class="info-key">亮度:</span>
                                    <span class="info-value highlight">${data.params.brightness || 0}</span>
                                </div>
                                <div class="info-row">
                                    <span class="info-key">对比度:</span>
                                    <span class="info-value highlight">${data.params.contrast || 0}</span>
                                </div>
                                <div class="info-row">
                                    <span class="info-key">饱和度:</span>
                                    <span class="info-value highlight">${data.params.saturation || 0}</span>
                                </div>
                            ` : ''}
                        </div>
                    </div>
                `;
            });

            statusHtml += '</div>';
            statusElement.innerHTML = statusHtml;

            // 添加样式
            const style = document.createElement('style');
            style.textContent = `
                .status-ok {
                    color: #4caf50 !important;
                    font-weight: 500;
                }
                .status-warning {
                    color: #ff9800 !important;
                    font-weight: 500;
                }
                .status-offline {
                    color: #9e9e9e !important;
                }
                .highlight {
                    color: #1976d2 !important;
                    font-weight: 500;
                }
                .active-camera-info {
                    background: #e8f5e9 !important;
                    border-left: 3px solid #4caf50 !important;
                }
            `;
            document.head.appendChild(style);
        })
        .catch(error => {
            console.error('获取摄像头列表失败:', error);
            statusElement.innerHTML = '<p class="error">获取摄像头信息失败: ' + error.message + '</p>';
        });
    }

    // 获取摄像头支持的最大分辨率
    function getMaxResolution(formats) {
        let maxPixels = 0;
        let maxResolution = '';

        Object.values(formats).forEach(resolutions => {
            resolutions.forEach(res => {
                const pixels = res.width * res.height;
                if (pixels > maxPixels) {
                    maxPixels = pixels;
                    maxResolution = `${res.width}x${res.height}`;
                }
            });
        });

        return maxResolution;
    }

    // Tab切换功能
    const tabs = document.querySelectorAll('.tab');
    const tabContents = document.querySelectorAll('.tab-content');
    let systemInfoInterval = null;  // 用于存储系统信息更新的定时器

    tabs.forEach(tab => {
        tab.addEventListener('click', () => {
            // 移除所有tab的active类
            tabs.forEach(t => t.classList.remove('active'));
            // 移除所有内容的active类
            tabContents.forEach(content => content.classList.remove('active'));
            
            // 添加当前tab的active类
            tab.classList.add('active');
            // 显示对应的内容
            const tabId = tab.getAttribute('data-tab');
            document.getElementById(tabId + 'Tab').classList.add('active');

            // 根据激活的标签页处理定时器
            if (tabId === 'system') {
                // 系统信息标签页被激活时，立即获取一次信息并启动定时更新
                getSystemSummary();
                systemInfoInterval = setInterval(getSystemSummary, 5000);
            } else {
                // 其他标签页被激活时，清除系统信息更新定时器
                if (systemInfoInterval) {
                    clearInterval(systemInfoInterval);
                    systemInfoInterval = null;
                }
            }
        });
    });

    // 获取API状态
    function getApiStatus() {
        fetch('/api/system/info')
        .then(response => {
            if (!response.ok) {
                throw new Error(`HTTP error! status: ${response.status}`);
            }
            return response.json();
        })
        .then(data => {
            if (data.status === 'success') {
                updateApiStatus(true);
            } else {
                updateApiStatus(false);
            }
        })
        .catch(error => {
            console.error('API请求失败:', error);
            updateApiStatus(false);
        });
    }
    
    // 获取摄像头状态
    function getCameraStatus() {
        fetch('/api/camera/status')
        .then(response => {
            if (!response.ok) {
                throw new Error(`HTTP error! status: ${response.status}`);
            }
            return response.json();
        })
        .then(data => {
            if (data.success === false) {
                throw new Error(data.error || '获取摄像头信息失败');
            }
            updateCameraStatus(data);
        })
        .catch(error => {
            console.error('获取摄像头信息失败:', error);
            const statusElement = document.getElementById('camera-status');
            if (statusElement) {
                statusElement.innerHTML = '<p class="error">获取摄像头信息失败: ' + error.message + '</p>';
            }
        });
    }
    
    // 获取摄像头列表
    function getCameraList() {
        fetch('/api/camera/list')
        .then(response => {
            if (!response.ok) {
                throw new Error(`HTTP error! status: ${response.status}`);
            }
            return response.json();
        })
        .then(data => {
            if (data.cameras && Array.isArray(data.cameras)) {
                const cameraSelect = document.getElementById('cameraSelect');
                cameraSelect.innerHTML = '<option value="">请选择摄像头</option>';
                data.cameras.forEach(camera => {
                    const option = document.createElement('option');
                    option.value = camera.path;
                    option.textContent = `${camera.name} (${camera.path})`;
                    cameraSelect.appendChild(option);
                });
            }
        })
        .catch(error => {
            console.error('获取摄像头列表失败:', error);
            const cameraSelect = document.getElementById('cameraSelect');
            cameraSelect.innerHTML = '<option value="">获取摄像头列表失败</option>';
        });
    }
    
    // 获取系统状态摘要
    function getSystemSummary() {
        fetch('/api/system/info')
        .then(response => {
            if (!response.ok) {
                throw new Error(`HTTP error! status: ${response.status}`);
            }
            return response.json();
        })
        .then(data => {
            if (data.status === 'success') {
                // 更新系统基本信息
                const hostnameElement = document.getElementById('hostname');
                if (hostnameElement) {
                    hostnameElement.textContent = data.system.hostname || '--';
                }

                const osVersionElement = document.getElementById('osVersion');
                if (osVersionElement) {
                    osVersionElement.textContent = data.system.os || '--';
                }

                const kernelVersionElement = document.getElementById('kernelVersion');
                if (kernelVersionElement) {
                    kernelVersionElement.textContent = data.system.kernel || '--';
                }

                const uptimeElement = document.getElementById('uptime');
                if (uptimeElement) {
                    uptimeElement.textContent = data.system.uptime || '--';
                }

                const systemTimeElement = document.getElementById('systemTime');
                if (systemTimeElement) {
                    systemTimeElement.textContent = data.system.system_time || '--';
                }

                // 更新CPU信息
                const cpuUsageElement = document.getElementById('cpuUsage');
                const cpuUsageBarElement = document.getElementById('cpuUsageBar');
                if (cpuUsageElement && data.cpu) {
                    const usagePercent = data.cpu.usage_percent.toFixed(1);
                    cpuUsageElement.textContent = `${usagePercent}%`;
                    if (cpuUsageBarElement) {
                        cpuUsageBarElement.style.width = `${usagePercent}%`;
                        if (usagePercent > 90) {
                            cpuUsageBarElement.className = 'progress-fill danger';
                        } else if (usagePercent > 70) {
                            cpuUsageBarElement.className = 'progress-fill warning';
                        } else {
                            cpuUsageBarElement.className = 'progress-fill';
                        }
                    }
                }

                const cpuTempElement = document.getElementById('cpuTemp');
                if (cpuTempElement && data.cpu) {
                    cpuTempElement.textContent = `${data.cpu.temperature.toFixed(1)}°C`;
                }

                const cpuFreqElement = document.getElementById('cpuFreq');
                if (cpuFreqElement && data.cpu) {
                    cpuFreqElement.textContent = `${data.cpu.frequency.toFixed(0)} MHz`;
                }

                const loadAverageElement = document.getElementById('loadAverage');
                if (loadAverageElement && data.system.load_average) {
                    loadAverageElement.textContent = data.system.load_average.map(load => load.toFixed(2)).join(', ');
                }

                // 更新GPU信息
                const gpuUsageElement = document.getElementById('gpuUsage');
                const gpuUsageBarElement = document.getElementById('gpuUsageBar');
                if (gpuUsageElement && data.gpu) {
                    const usagePercent = data.gpu.usage_percent.toFixed(1);
                    gpuUsageElement.textContent = `${usagePercent}%`;
                    if (gpuUsageBarElement) {
                        gpuUsageBarElement.style.width = `${usagePercent}%`;
                        if (usagePercent > 90) {
                            gpuUsageBarElement.className = 'progress-fill danger';
                        } else if (usagePercent > 70) {
                            gpuUsageBarElement.className = 'progress-fill warning';
                        } else {
                            gpuUsageBarElement.className = 'progress-fill';
                        }
                    }
                }

                const gpuTempElement = document.getElementById('gpuTemp');
                if (gpuTempElement && data.gpu) {
                    gpuTempElement.textContent = `${data.gpu.temperature.toFixed(1)}°C`;
                }

                const gpuMemUsageElement = document.getElementById('gpuMemUsage');
                const gpuMemUsageBarElement = document.getElementById('gpuMemUsageBar');
                if (gpuMemUsageElement && data.gpu) {
                    const memUsagePercent = data.gpu.memory_usage_percent.toFixed(1);
                    gpuMemUsageElement.textContent = `${memUsagePercent}%`;
                    if (gpuMemUsageBarElement) {
                        gpuMemUsageBarElement.style.width = `${memUsagePercent}%`;
                        if (memUsagePercent > 90) {
                            gpuMemUsageBarElement.className = 'progress-fill danger';
                        } else if (memUsagePercent > 70) {
                            gpuMemUsageBarElement.className = 'progress-fill warning';
                        } else {
                            gpuMemUsageBarElement.className = 'progress-fill';
                        }
                    }
                }

                const gpuFreqElement = document.getElementById('gpuFreq');
                if (gpuFreqElement && data.gpu) {
                    gpuFreqElement.textContent = `${data.gpu.frequency.toFixed(0)} MHz`;
                }

                // 更新内存信息
                function formatBytes(bytes) {
                    if (bytes === 0) return '0 B';
                    const k = 1024;
                    const sizes = ['B', 'KB', 'MB', 'GB', 'TB'];
                    const i = Math.floor(Math.log(bytes) / Math.log(k));
                    return `${(bytes / Math.pow(k, i)).toFixed(2)} ${sizes[i]}`;
                }

                const memTotalElement = document.getElementById('memTotal');
                if (memTotalElement && data.memory) {
                    memTotalElement.textContent = formatBytes(data.memory.total);
                }

                const memUsedElement = document.getElementById('memUsed');
                if (memUsedElement && data.memory) {
                    memUsedElement.textContent = formatBytes(data.memory.used);
                }

                const memFreeElement = document.getElementById('memFree');
                if (memFreeElement && data.memory) {
                    memFreeElement.textContent = formatBytes(data.memory.free);
                }

                const memUsageElement = document.getElementById('memUsage');
                const memUsageBarElement = document.getElementById('memUsageBar');
                if (memUsageElement && data.memory) {
                    const usagePercent = data.memory.usage_percent.toFixed(1);
                    memUsageElement.textContent = `${usagePercent}%`;
                    if (memUsageBarElement) {
                        memUsageBarElement.style.width = `${usagePercent}%`;
                        if (usagePercent > 90) {
                            memUsageBarElement.className = 'progress-fill danger';
                        } else if (usagePercent > 70) {
                            memUsageBarElement.className = 'progress-fill warning';
                        } else {
                            memUsageBarElement.className = 'progress-fill';
                        }
                    }
                }

                // 更新存储信息
                if (data.storage && data.storage.length > 0) {
                    const storage = data.storage[0]; // 使用第一个存储设备的信息

                    const diskTotalElement = document.getElementById('diskTotal');
                    if (diskTotalElement) {
                        diskTotalElement.textContent = formatBytes(storage.total);
                    }

                    const diskUsedElement = document.getElementById('diskUsed');
                    if (diskUsedElement) {
                        diskUsedElement.textContent = formatBytes(storage.used);
                    }

                    const diskFreeElement = document.getElementById('diskFree');
                    if (diskFreeElement) {
                        diskFreeElement.textContent = formatBytes(storage.free);
                    }

                    const diskUsageElement = document.getElementById('diskUsage');
                    const diskUsageBarElement = document.getElementById('diskUsageBar');
                    if (diskUsageElement) {
                        const usagePercent = storage.usage_percent.toFixed(1);
                        diskUsageElement.textContent = `${usagePercent}%`;
                        if (diskUsageBarElement) {
                            diskUsageBarElement.style.width = `${usagePercent}%`;
                            if (usagePercent > 90) {
                                diskUsageBarElement.className = 'progress-fill danger';
                            } else if (usagePercent > 70) {
                                diskUsageBarElement.className = 'progress-fill warning';
                            } else {
                                diskUsageBarElement.className = 'progress-fill';
                            }
                        }
                    }
                }

                // 更新网络信息
                if (data.network && data.network.length > 0) {
                    const network = data.network[0]; // 使用第一个网络接口的信息

                    const ipAddressElement = document.getElementById('ipAddress');
                    if (ipAddressElement) {
                        ipAddressElement.textContent = network.ip_address || '--';
                    }

                    // 更新网络接口信息
                    const networkInterfacesElement = document.getElementById('networkInterfaces');
                    if (networkInterfacesElement) {
                        networkInterfacesElement.innerHTML = data.network.map(iface => `
                            <div class="info-item">
                                <span class="info-label">${iface.interface}:</span>
                                <span class="info-value">${iface.ip_address}</span>
                            </div>
                            <div class="info-item">
                                <span class="info-label">接收:</span>
                                <span class="info-value">${formatBytes(iface.rx_bytes)}/s</span>
                            </div>
                            <div class="info-item">
                                <span class="info-label">发送:</span>
                                <span class="info-value">${formatBytes(iface.tx_bytes)}/s</span>
                            </div>
                        `).join('');
                    }
                }
            }
        })
        .catch(error => {
            console.error('获取系统信息失败:', error);
        });
    }
    
    // 添加样式
    const style = document.createElement('style');
    style.textContent = `
        .status {
            padding: 10px 15px;
            border-radius: 4px;
            margin: 10px 0;
            font-size: 14px;
            line-height: 1.5;
        }
        .status.error {
            background-color: #fdecea;
            color: #932419;
            border: 1px solid #f5c6c3;
        }
        .status.success {
            background-color: #e8f5e9;
            color: #1b5e20;
            border: 1px solid #c8e6c9;
        }
        .status.info {
            background-color: #e3f2fd;
            color: #0d47a1;
            border: 1px solid #bbdefb;
        }
        .camera-status-container {
            display: flex;
            flex-direction: column;
            gap: 15px;
            margin: 10px 0;
            font-size: 12px;
        }
        .camera-info {
            background: #f8f9fa;
            border-radius: 6px;
            padding: 12px;
            box-shadow: 0 1px 3px rgba(0,0,0,0.1);
        }
        .camera-info.active-camera-info {
            background: #e8f5e9;
            border-left: 3px solid #4caf50;
        }
        .camera-info-header {
            margin-bottom: 8px;
            padding-bottom: 8px;
            border-bottom: 1px solid #eee;
        }
        .camera-info-header h4 {
            margin: 0;
            font-size: 13px;
            color: #333;
        }
        .device-path {
            font-size: 11px;
            color: #666;
        }
        .camera-info-grid {
            display: grid;
            grid-template-columns: repeat(auto-fill, minmax(200px, 1fr));
            gap: 8px;
        }
        .info-row {
            display: flex;
            justify-content: space-between;
            align-items: center;
            padding: 2px 0;
        }
        .info-key {
            color: #666;
            font-size: 11px;
        }
        .info-value {
            color: #333;
            font-size: 11px;
        }
        .info-value.highlight {
            color: #1976d2;
            font-weight: 500;
        }
        .status-ok {
            color: #4caf50;
            font-weight: 500;
        }
        .status-warning {
            color: #ff9800;
            font-weight: 500;
        }
        .status-error {
            color: #f44336;
            font-weight: 500;
        }
    `;
    document.head.appendChild(style);

    // 绑定摄像头选择事件
    const cameraSelect = document.getElementById('cameraSelect');
    if (cameraSelect) {
        console.log('正在绑定摄像头选择事件...');
        
        // 在点击下拉框时获取摄像头列表
        cameraSelect.addEventListener('mousedown', function(e) {
            // 如果下拉框为空或只有一个选项（加载中...），则获取摄像头列表
            if (this.options.length <= 1) {
                getCameraList();
            }
        });

        cameraSelect.addEventListener('change', function(e) {
            const devicePath = e.target.value;
            console.log('摄像头选择改变:', devicePath);
            selectedCamera = devicePath; // 更新选中的摄像头
            const resolutionSelect = document.getElementById('resolutionSelect');
            
            if (devicePath) {
                console.log('开始获取摄像头分辨率...');
                // 获取摄像头支持的分辨率
                fetch('/api/camera/list')
                .then(response => {
                    if (!response.ok) {
                        throw new Error(`HTTP error! status: ${response.status}`);
                    }
                    return response.json();
                })
                .then(data => {
                    console.log('获取到摄像头列表数据:', data);
                    if (data.cameras) {
                        const camera = data.cameras.find(cam => cam.path === devicePath);
                        console.log('找到匹配的摄像头:', camera);
                        
                        if (camera && camera.formats) {
                            // 清空分辨率选择框
                            resolutionSelect.innerHTML = '<option value="">请选择分辨率</option>';
                            
                            // 获取所有支持的分辨率
                            const resolutions = new Set();
                            Object.entries(camera.formats).forEach(([format, formatResolutions]) => {
                                console.log(`处理格式 ${format} 的分辨率:`, formatResolutions);
                                formatResolutions.forEach(res => {
                                    resolutions.add(`${res.width}x${res.height}`);
                                });
                            });

                            // 将分辨率转换为数组并排序
                            const sortedResolutions = Array.from(resolutions).sort((a, b) => {
                                const [aWidth, aHeight] = a.split('x').map(Number);
                                const [bWidth, bHeight] = b.split('x').map(Number);
                                // 按照总像素数从大到小排序
                                return (bWidth * bHeight) - (aWidth * aHeight);
                            });

                            console.log('排序后的分辨率:', sortedResolutions);

                            // 添加分辨率选项
                            sortedResolutions.forEach(resolution => {
                                const option = document.createElement('option');
                                option.value = resolution;
                                option.textContent = resolution;
                                resolutionSelect.appendChild(option);
                            });

                            // 启用分辨率选择框
                            resolutionSelect.disabled = false;
                        } else {
                            console.error('未找到摄像头格式信息');
                            resolutionSelect.innerHTML = '<option value="">该摄像头没有可用的分辨率</option>';
                            resolutionSelect.disabled = true;
                        }
                    } else {
                        throw new Error('获取摄像头列表失败');
                    }
                })
                .catch(error => {
                    console.error('获取摄像头分辨率失败:', error);
                    resolutionSelect.innerHTML = '<option value="">获取分辨率失败</option>';
                    resolutionSelect.disabled = true;
                });
            } else {
                // 清空并禁用分辨率选择框
                resolutionSelect.innerHTML = '<option value="">请先选择摄像头</option>';
                resolutionSelect.disabled = true;
            }
        });
        console.log('摄像头选择事件绑定完成');
    } else {
        console.error('未找到摄像头选择元素');
    }

    // 初始化：获取各种状态
    getApiStatus();
    getCameraStatus();
    // 只在系统信息标签页激活时才获取系统信息
    if (document.querySelector('.tab[data-tab="system"]').classList.contains('active')) {
        getSystemSummary();
        systemInfoInterval = setInterval(getSystemSummary, 5000);
    }
    
    // 定期更新状态
    setInterval(getApiStatus, 30000);  // 每30秒更新一次API状态
    setInterval(getCameraStatus, 10000);  // 每10秒更新一次摄像头状态

    // 打开摄像头
    function openCamera(devicePath, format, width, height, fps) {
        selectedCamera = devicePath; // 更新选中的摄像头
        const data = {
            device_path: devicePath,
            format: format,
            width: width,
            height: height,
            fps: fps
        };

        fetch('/api/camera/open', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify(data)
        })
        .then(response => {
            if (!response.ok) {
                throw new Error(`HTTP error! status: ${response.status}`);
            }
            return response.json();
        })
        .then(data => {
            if (data.status === 'success') {
                console.log('摄像头已打开');
                // 更新UI状态
                document.getElementById('startPreviewBtn').disabled = false;
                document.getElementById('stopPreviewBtn').disabled = true;
                document.getElementById('captureBtn').disabled = true;
                document.getElementById('startRecordingBtn').disabled = true;
                document.getElementById('stopRecordingBtn').disabled = true;
                // 更新摄像头状态
                getCameraStatus();
            } else {
                throw new Error(data.message || '打开摄像头失败');
            }
        })
        .catch(error => {
            console.error('打开摄像头失败:', error);
            showError('打开摄像头失败: ' + error.message);
        });
    }

    // 关闭摄像头
    function closeCamera() {
        selectedCamera = null; // 清除选中的摄像头
        fetch('/api/camera/close', {
            method: 'POST'
        })
        .then(response => {
            if (!response.ok) {
                throw new Error(`HTTP error! status: ${response.status}`);
            }
            return response.json();
        })
        .then(data => {
            if (data.success) {
                console.log('摄像头已关闭');
                // 更新UI状态
                document.getElementById('startPreviewBtn').disabled = false;
                document.getElementById('stopPreviewBtn').disabled = true;
                document.getElementById('captureBtn').disabled = true;
                document.getElementById('startRecordingBtn').disabled = true;
                document.getElementById('stopRecordingBtn').disabled = true;
                // 更新摄像头状态
                getCameraStatus();
            } else {
                throw new Error(data.error || '关闭摄像头失败');
            }
        })
        .catch(error => {
            console.error('关闭摄像头失败:', error);
            showError('关闭摄像头失败: ' + error.message);
        });
    }

    // 开始预览
    function startPreview() {
        fetch('/api/camera/start_preview', {
            method: 'POST'
        })
        .then(response => {
            if (!response.ok) {
                throw new Error(`HTTP error! status: ${response.status}`);
            }
            return response.json();
        })
        .then(data => {
            if (data.status === 'success') {
                console.log('预览已开始');
                // 更新UI状态
                document.getElementById('startPreviewBtn').disabled = true;
                document.getElementById('stopPreviewBtn').disabled = false;
                document.getElementById('captureBtn').disabled = false;
                document.getElementById('startRecordingBtn').disabled = false;
                // 隐藏占位符
                document.getElementById('previewPlaceholder').style.display = 'none';
                // 开始MJPEG流
                startMjpegStream();
                // 立即更新摄像头状态
                getCameraStatus();
            } else {
                throw new Error(data.message || '开始预览失败');
            }
        })
        .catch(error => {
            console.error('开始预览失败:', error);
            showError('开始预览失败: ' + error.message);
        });
    }

    // 停止预览
    function stopPreview() {
        fetch('/api/camera/stop_preview', {
            method: 'POST'
        })
        .then(response => {
            if (!response.ok) {
                throw new Error(`HTTP error! status: ${response.status}`);
            }
            return response.json();
        })
        .then(data => {
            if (data.status === 'success') {
                console.log('预览已停止');
                // 更新UI状态
                document.getElementById('startPreviewBtn').disabled = false;
                document.getElementById('stopPreviewBtn').disabled = true;
                document.getElementById('captureBtn').disabled = true;
                document.getElementById('startRecordingBtn').disabled = true;
                document.getElementById('stopRecordingBtn').disabled = true;
                // 显示占位符
                document.getElementById('previewPlaceholder').style.display = 'flex';
                // 停止MJPEG流
                stopMjpegStream();
                // 立即更新摄像头状态
                getCameraStatus();
            } else {
                throw new Error(data.message || '停止预览失败');
            }
        })
        .catch(error => {
            console.error('停止预览失败:', error);
            showError('停止预览失败: ' + error.message);
        });
    }

    // 显示错误信息
    function showError(message) {
        const statusElement = document.getElementById('status');
        statusElement.innerHTML = message.replace(/\n/g, '<br>');
        statusElement.className = 'status error';
        statusElement.style.display = 'block';
        // 对于重要错误，不自动隐藏
        if (!message.includes('可能的原因')) {
            setTimeout(() => {
                statusElement.style.display = 'none';
            }, 5000);
        }
    }

    // 显示成功信息
    function showSuccess(message) {
        const statusElement = document.getElementById('status');
        statusElement.textContent = message;
        statusElement.className = 'status success';
        statusElement.style.display = 'block';
        setTimeout(() => {
            statusElement.style.display = 'none';
        }, 3000);
    }

    // 显示状态信息
    function showStatus(message) {
        const statusElement = document.getElementById('status');
        statusElement.textContent = message;
        statusElement.className = 'status info';
        statusElement.style.display = 'block';
    }

    // 拍照功能
    function captureImage() {
        fetch('/api/camera/capture', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify({
                quality: 90
            })
        })
        .then(response => {
            if (!response.ok) {
                throw new Error(`HTTP error! status: ${response.status}`);
            }
            return response.json();
        })
        .then(data => {
            if (data.status === 'success') {
                console.log('拍照成功:', data.filename);
                showSuccess('拍照成功: ' + data.filename);
            } else {
                throw new Error(data.message || '拍照失败');
            }
        })
        .catch(error => {
            console.error('拍照失败:', error);
            showError('拍照失败: ' + error.message);
        });
    }

    // 开始录制
    function startRecording() {
        fetch('/api/camera/start_recording', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify({
                format: 'mp4',
                encoder: 'h264_rkmpp',
                bitrate: 4000000
            })
        })
        .then(response => {
            if (!response.ok) {
                throw new Error(`HTTP error! status: ${response.status}`);
            }
            return response.json();
        })
        .then(data => {
            if (data.success) {
                console.log('录制已开始');
                // 更新UI状态
                document.getElementById('startRecordingBtn').disabled = true;
                document.getElementById('stopRecordingBtn').disabled = false;
                // 显示录制状态
                document.getElementById('recordingStatus').style.display = 'block';
                // 开始更新录制时间
                startRecordingTimer();
            } else {
                throw new Error(data.error || '开始录制失败');
            }
        })
        .catch(error => {
            console.error('开始录制失败:', error);
            showError('开始录制失败: ' + error.message);
        });
    }

    // 停止录制
    function stopRecording() {
        fetch('/api/camera/stop_recording', {
            method: 'POST'
        })
        .then(response => {
            if (!response.ok) {
                throw new Error(`HTTP error! status: ${response.status}`);
            }
            return response.json();
        })
        .then(data => {
            if (data.success) {
                console.log('录制已停止:', data.file_path);
                // 更新UI状态
                document.getElementById('startRecordingBtn').disabled = false;
                document.getElementById('stopRecordingBtn').disabled = true;
                // 隐藏录制状态
                document.getElementById('recordingStatus').style.display = 'none';
                // 停止更新录制时间
                stopRecordingTimer();
                showSuccess('录制已完成: ' + data.file_path);
            } else {
                throw new Error(data.error || '停止录制失败');
            }
        })
        .catch(error => {
            console.error('停止录制失败:', error);
            showError('停止录制失败: ' + error.message);
        });
    }

    // 录制计时器
    let recordingTimer = null;
    let recordingStartTime = 0;

    function startRecordingTimer() {
        recordingStartTime = Date.now();
        recordingTimer = setInterval(updateRecordingTime, 1000);
    }

    function stopRecordingTimer() {
        if (recordingTimer) {
            clearInterval(recordingTimer);
            recordingTimer = null;
        }
    }

    function updateRecordingTime() {
        const elapsed = Math.floor((Date.now() - recordingStartTime) / 1000);
        const minutes = Math.floor(elapsed / 60).toString().padStart(2, '0');
        const seconds = (elapsed % 60).toString().padStart(2, '0');
        document.getElementById('recordingTime').textContent = `${minutes}:${seconds}`;
    }

    // MJPEG流控制
    let mjpegImage = null;
    let streamRetryCount = 0;
    const MAX_RETRY_COUNT = 3;

    function startMjpegStream() {
        const preview = document.getElementById('preview');
        
        // 检查是否选择了摄像头
        if (!selectedCamera) {
            console.error('未选择摄像头');
            showError('未选择摄像头，无法启动预览');
            return;
        }

        const clientId = generateClientId();
        const streamUrl = `/api/camera/mjpeg?camera_id=${encodeURIComponent(selectedCamera)}&client_id=${clientId}&t=${new Date().getTime()}`;
        
        console.log('开始MJPEG流，URL:', streamUrl);
        showStatus('正在连接MJPEG流...');
        
        // 创建新的Image对象
        mjpegImage = new Image();
        
        // 设置超时处理
        let loadTimeout = setTimeout(() => {
            console.error('MJPEG流加载超时');
            if (mjpegImage) {
                mjpegImage.onerror(new Error('加载超时'));
            }
        }, 10000); // 10秒超时
        
        mjpegImage.onload = function() {
            clearTimeout(loadTimeout);
            console.log('MJPEG流加载成功');
            preview.src = this.src;
            streamRetryCount = 0; // 重置重试计数
            showSuccess('预览画面加载成功');
        };
        
        mjpegImage.onerror = function(error) {
            clearTimeout(loadTimeout);
            console.error('MJPEG流加载失败:', error);
            
            // 检查网络连接
            if (!navigator.onLine) {
                showError('网络连接已断开，请检查网络连接后重试');
                stopPreview();
                return;
            }
            
            // 检查摄像头状态
            fetch('/api/camera/status')
            .then(response => response.json())
            .then(data => {
                if (!data.is_capturing) {
                    showError('摄像头未在预览状态，请重新启动预览');
                    stopPreview();
                    return;
                }
                
                if (streamRetryCount < MAX_RETRY_COUNT) {
                    streamRetryCount++;
                    console.log(`尝试重新连接MJPEG流 (${streamRetryCount}/${MAX_RETRY_COUNT})`);
                    showError(`MJPEG流连接失败，正在重试 (${streamRetryCount}/${MAX_RETRY_COUNT})`);
                    
                    // 使用指数退避策略
                    const retryDelay = Math.min(1000 * Math.pow(2, streamRetryCount - 1), 5000);
                    setTimeout(() => {
                        if (mjpegImage) {
                            mjpegImage.src = streamUrl + '&retry=' + streamRetryCount;
                        }
                    }, retryDelay);
                } else {
                    const errorMsg = '摄像头MJPEG流加载失败。\n可能的原因：\n' +
                                   '1. 摄像头不支持MJPEG格式\n' +
                                   '2. 摄像头连接不稳定\n' +
                                   '3. 网络连接问题\n' +
                                   '4. 服务器资源不足\n' +
                                   '请检查以下内容：\n' +
                                   '- 确认摄像头支持MJPEG格式\n' +
                                   '- 检查USB连接是否稳定\n' +
                                   '- 确认网络连接正常\n' +
                                   '- 尝试重新插拔摄像头\n' +
                                   '- 检查系统日志获取详细错误信息';
                    showError(errorMsg);
                    stopPreview();
                }
            })
            .catch(error => {
                console.error('获取摄像头状态失败:', error);
                showError('获取摄像头状态失败，请检查服务器连接');
                stopPreview();
            });
        };
        
        // 设置预览图像的样式
        preview.style.width = '100%';
        preview.style.height = '100%';
        preview.style.objectFit = 'contain';
        
        // 开始加载MJPEG流
        console.log('开始加载MJPEG流，设备:', selectedCamera);
        mjpegImage.src = streamUrl;
    }

    function stopMjpegStream() {
        if (mjpegImage) {
            console.log('停止MJPEG流');
            mjpegImage.src = '';
            mjpegImage = null;
        }
        streamRetryCount = 0; // 重置重试计数
        document.getElementById('preview').src = 'data:image/gif;base64,R0lGODlhAQABAAAAACH5BAEKAAEALAAAAAABAAEAAAICTAEAOw==';
    }

    // 生成客户端ID
    function generateClientId() {
        return 'client-' + Math.random().toString(36).substr(2, 9);
    }

    // 绑定应用设置按钮事件
    const applySettingsBtn = document.getElementById('applySettingsBtn');
    if (applySettingsBtn) {
        console.log('正在绑定应用设置按钮事件...');
        applySettingsBtn.addEventListener('click', function() {
            const devicePath = document.getElementById('cameraSelect').value;
            const resolutionSelect = document.getElementById('resolutionSelect');
            const fpsInput = document.getElementById('fpsInput');
            
            if (devicePath && resolutionSelect.value) {
                const [width, height] = resolutionSelect.value.split('x').map(Number);
                const fps = parseInt(fpsInput.value) || 30;
                
                console.log('应用新的摄像头设置:', {
                    devicePath,
                    width,
                    height,
                    fps
                });

                // 关闭当前摄像头
                closeCamera();
                
                // 等待1秒后重新打开摄像头
                setTimeout(() => {
                    openCamera(devicePath, 'MJPG', width, height, fps);
                }, 1000);
            } else {
                showError('请选择摄像头和分辨率');
            }
        });
        console.log('应用设置按钮事件绑定完成');
    } else {
        console.error('未找到应用设置按钮');
    }

    // 绑定按钮事件
    document.getElementById('startPreviewBtn').addEventListener('click', startPreview);
    document.getElementById('stopPreviewBtn').addEventListener('click', stopPreview);
    document.getElementById('captureBtn').addEventListener('click', captureImage);
    document.getElementById('startRecordingBtn').addEventListener('click', startRecording);
    document.getElementById('stopRecordingBtn').addEventListener('click', stopRecording);
});
