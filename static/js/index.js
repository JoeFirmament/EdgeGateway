// 全局变量
let selectedCamera = '';
let selectedFormat = 'MJPG';
let selectedWidth = 640;
let selectedHeight = 480;
let isRecording = false;
let isPreviewing = false;
let recordingStartTime = 0;
let recordingTimer = null;
let systemInfoTimer = null;

// DOM元素
const tabs = document.querySelectorAll('.tab');
const tabContents = document.querySelectorAll('.tab-content');
const clockElement = document.getElementById('clock');

// 摄像头控制元素
const cameraSelect = document.getElementById('cameraSelect');
const resolutionSelect = document.getElementById('resolutionSelect');
const fpsInput = document.getElementById('fpsInput');
const applySettingsBtn = document.getElementById('applySettingsBtn');
const captureBtn = document.getElementById('captureBtn');
const startRecordingBtn = document.getElementById('startRecordingBtn');
const stopRecordingBtn = document.getElementById('stopRecordingBtn');
const startPreviewBtn = document.getElementById('startPreviewBtn');
const stopPreviewBtn = document.getElementById('stopPreviewBtn');
const statusElement = document.getElementById('status');
const recordingStatus = document.getElementById('recordingStatus');
const recordingTime = document.getElementById('recordingTime');
const previewImg = document.getElementById('preview');
const previewPlaceholder = document.getElementById('previewPlaceholder');

// 摄像头状态元素
const connectionStatusElement = document.getElementById('connectionStatus');
const clientIdElement = document.getElementById('clientId');
const currentResolutionElement = document.getElementById('currentResolution');
const currentFpsElement = document.getElementById('currentFps');

// 系统信息元素
const hostnameElement = document.getElementById('hostname');
const osVersionElement = document.getElementById('osVersion');
const kernelVersionElement = document.getElementById('kernelVersion');
const uptimeElement = document.getElementById('uptime');
const systemTimeElement = document.getElementById('systemTime');

// CPU信息元素
const cpuInfoElement = document.getElementById('cpuInfo');
const cpuUsageElement = document.getElementById('cpuUsage');
const cpuUsageBarElement = document.getElementById('cpuUsageBar');
const cpuTempElement = document.getElementById('cpuTemp');
const cpuFreqElement = document.getElementById('cpuFreq');
const loadAverageElement = document.getElementById('loadAverage');

// GPU信息元素
const gpuUsageElement = document.getElementById('gpuUsage');
const gpuUsageBarElement = document.getElementById('gpuUsageBar');
const gpuTempElement = document.getElementById('gpuTemp');
const gpuMemUsageElement = document.getElementById('gpuMemUsage');
const gpuMemUsageBarElement = document.getElementById('gpuMemUsageBar');
const gpuFreqElement = document.getElementById('gpuFreq');

// 内存信息元素
const memTotalElement = document.getElementById('memTotal');
const memUsedElement = document.getElementById('memUsed');
const memFreeElement = document.getElementById('memFree');
const memUsageElement = document.getElementById('memUsage');
const memUsageBarElement = document.getElementById('memUsageBar');

// 存储信息元素
const diskTotalElement = document.getElementById('diskTotal');
const diskUsedElement = document.getElementById('diskUsed');
const diskFreeElement = document.getElementById('diskFree');
const diskUsageElement = document.getElementById('diskUsage');
const diskUsageBarElement = document.getElementById('diskUsageBar');

// 网络信息元素
const ipAddressElement = document.getElementById('ipAddress');
const wifiSsidElement = document.getElementById('wifiSsid');
const networkInterfacesElement = document.getElementById('networkInterfaces');

// 初始化
document.addEventListener('DOMContentLoaded', () => {
    // 初始化标签页
    initTabs();

    // 初始化时钟
    updateClock();
    setInterval(updateClock, 1000);

    // 加载系统信息
    loadSystemInfo();
    setInterval(loadSystemInfo, 5000); // 每5秒更新一次

    // 加载摄像头列表
    loadCameras();

    // 加载摄像头连接状态
    loadCameraConnectionStatus();
    setInterval(loadCameraConnectionStatus, 3000); // 每3秒更新一次

    // 初始化UI状态
    updateUIState();

    // 事件监听器
    cameraSelect.addEventListener('change', onCameraChange);
    applySettingsBtn.addEventListener('click', applySettings);
    captureBtn.addEventListener('click', captureImage);
    startRecordingBtn.addEventListener('click', startRecording);
    stopRecordingBtn.addEventListener('click', stopRecording);
    startPreviewBtn.addEventListener('click', startPreview);
    stopPreviewBtn.addEventListener('click', stopPreview);
});

// 初始化标签页
function initTabs() {
    tabs.forEach(tab => {
        tab.addEventListener('click', () => {
            // 移除所有标签页的active类
            tabs.forEach(t => t.classList.remove('active'));
            tabContents.forEach(c => c.classList.remove('active'));

            // 添加当前标签页的active类
            tab.classList.add('active');
            const tabName = tab.getAttribute('data-tab');
            document.getElementById(`${tabName}Tab`).classList.add('active');
        });
    });
}

// 更新时钟
function updateClock() {
    const now = new Date();
    const hours = now.getHours().toString().padStart(2, '0');
    const minutes = now.getMinutes().toString().padStart(2, '0');
    const seconds = now.getSeconds().toString().padStart(2, '0');
    clockElement.textContent = `${hours}:${minutes}:${seconds}`;
}

// 更新UI状态
function updateUIState() {
    // 根据预览状态更新按钮状态
    startPreviewBtn.disabled = isPreviewing;
    stopPreviewBtn.disabled = !isPreviewing;

    // 根据预览状态更新拍照和录制按钮状态
    captureBtn.disabled = !isPreviewing;
    startRecordingBtn.disabled = !isPreviewing || isRecording;
    stopRecordingBtn.disabled = !isRecording;

    // 根据预览状态更新预览占位符显示
    if (isPreviewing) {
        previewPlaceholder.style.display = 'none';
    } else {
        previewPlaceholder.style.display = 'flex';
        previewImg.src = 'data:image/gif;base64,R0lGODlhAQABAAAAACH5BAEKAAEALAAAAAABAAEAAAICTAEAOw==';
    }
}

// 加载摄像头连接状态
async function loadCameraConnectionStatus() {
    try {
        // 获取当前选中的摄像头ID
        const selectedCameraId = cameraSelect.value;
        if (!selectedCameraId) {
            updateConnectionStatus(false);
            return;
        }

        // 获取摄像头连接状态
        const response = await fetch(`/api/cameras/status?camera_id=${encodeURIComponent(selectedCameraId)}`);
        const data = await response.json();

        console.log('摄像头状态API响应:', data);

        if (data && data.success) {
            if (data.is_connected) {
                // 摄像头已连接
                updateConnectionStatus(true, data);
            } else {
                // 摄像头未连接
                updateConnectionStatus(false);
            }
        } else {
            // 获取状态失败
            console.error('获取摄像头状态失败:', data);
            updateConnectionStatus(false);
        }
    } catch (error) {
        console.error('加载摄像头连接状态失败:', error);
        updateConnectionStatus(false);
    }
}

// 更新连接状态显示
function updateConnectionStatus(isConnected, data = null) {
    console.log('更新连接状态:', isConnected, data);

    if (isConnected && data) {
        // 更新连接状态
        connectionStatusElement.textContent = '已连接';
        connectionStatusElement.className = 'status-value connected';

        // 更新客户端ID
        clientIdElement.textContent = data.client_id || '--';

        // 更新分辨率
        if (data.width && data.height) {
            currentResolutionElement.textContent = `${data.width} x ${data.height}`;
        } else {
            currentResolutionElement.textContent = '--';
        }

        // 更新帧率
        currentFpsElement.textContent = data.fps ? `${data.fps.toFixed(1)} FPS` : '--';
    } else {
        // 如果摄像头正在预览但API返回未连接，我们手动设置连接状态
        if (isPreviewing) {
            // 获取当前分辨率
            const [width, height] = resolutionSelect.value.split('x').map(Number);
            const fps = parseInt(fpsInput.value) || 30;

            // 从预览图像URL中提取客户端ID
            let clientId = '预览中';
            try {
                const imgSrc = previewImg.src;
                if (imgSrc.includes('client_id=')) {
                    const urlParams = new URLSearchParams(imgSrc.split('?')[1]);
                    const extractedClientId = urlParams.get('client_id');
                    if (extractedClientId) {
                        clientId = extractedClientId;
                    }
                }
            } catch (e) {
                console.log('无法从URL中提取客户端ID:', e);
            }

            connectionStatusElement.textContent = '已连接';
            connectionStatusElement.className = 'status-value connected';
            clientIdElement.textContent = clientId;
            currentResolutionElement.textContent = `${width} x ${height}`;
            currentFpsElement.textContent = `${fps} FPS`;

            console.log('手动设置连接状态:', width, height, fps, clientId);
        } else {
            // 未连接状态
            connectionStatusElement.textContent = '未连接';
            connectionStatusElement.className = 'status-value disconnected';
            clientIdElement.textContent = '--';
            currentResolutionElement.textContent = '--';
            currentFpsElement.textContent = '--';
        }
    }
}

// 加载摄像头列表
async function loadCameras() {
    try {
        const response = await fetch('/api/cameras');
        const data = await response.json();

        cameraSelect.innerHTML = '';

        if (data.cameras && data.cameras.length > 0) {
            data.cameras.forEach(camera => {
                const option = document.createElement('option');
                option.value = camera.path;
                option.textContent = `${camera.name} (${camera.path})`;
                option.dataset.formats = JSON.stringify(camera.formats);
                cameraSelect.appendChild(option);
            });

            // 选择第一个摄像头
            cameraSelect.value = data.cameras[0].path;
            onCameraChange();
        } else {
            cameraSelect.innerHTML = '<option value="">未找到摄像头</option>';
        }
    } catch (error) {
        showStatus(`加载摄像头失败: ${error.message}`, true);
    }
}

// 摄像头变更处理
function onCameraChange() {
    const selectedOption = cameraSelect.options[cameraSelect.selectedIndex];

    if (selectedOption && selectedOption.dataset.formats) {
        selectedCamera = selectedOption.value;
        const formats = JSON.parse(selectedOption.dataset.formats);

        resolutionSelect.innerHTML = '';
        resolutionSelect.disabled = false;

        // 优先使用MJPG格式
        if (formats.MJPG) {
            selectedFormat = 'MJPG';
            populateResolutions(formats.MJPG);
        } else {
            // 使用第一个可用格式
            const firstFormat = Object.keys(formats)[0];
            if (firstFormat) {
                selectedFormat = firstFormat;
                populateResolutions(formats[firstFormat]);
            } else {
                resolutionSelect.innerHTML = '<option value="">未找到支持的分辨率</option>';
                resolutionSelect.disabled = true;
            }
        }
    } else {
        resolutionSelect.innerHTML = '<option value="">请先选择摄像头</option>';
        resolutionSelect.disabled = true;
    }
}

// 填充分辨率选项
function populateResolutions(resolutions) {
    resolutions.forEach(res => {
        const option = document.createElement('option');
        option.value = `${res.width}x${res.height}`;
        option.textContent = `${res.width} x ${res.height}`;
        resolutionSelect.appendChild(option);
    });

    // 默认选择第一个分辨率
    if (resolutions.length > 0) {
        const firstRes = resolutions[0];
        selectedWidth = firstRes.width;
        selectedHeight = firstRes.height;
        resolutionSelect.value = `${selectedWidth}x${selectedHeight}`;
    }
}

// 应用摄像头设置
async function applySettings() {
    if (!selectedCamera) {
        showStatus('请先选择摄像头', true);
        return;
    }

    const resValue = resolutionSelect.value;
    if (!resValue) {
        showStatus('请选择分辨率', true);
        return;
    }

    // 如果当前正在预览，需要先停止预览
    if (isPreviewing) {
        console.log('应用设置前先停止预览...');
        await stopPreview();

        // 等待一小段时间，确保预览完全停止
        await new Promise(resolve => setTimeout(resolve, 1000));
    }

    // 确保摄像头已关闭
    try {
        console.log('确保摄像头已关闭...');
        await fetch('/api/cameras/stop_preview', {
            method: 'POST'
        });
    } catch (e) {
        console.log('停止预览时出错，可能没有活跃的预览:', e);
    }

    showStatus('摄像头设置已应用，请点击"开始预览"按钮查看画面');
}

// 开始预览
async function startPreview() {
    if (isPreviewing) return;

    if (!selectedCamera) {
        showStatus('请先选择摄像头', true);
        return;
    }

    const resValue = resolutionSelect.value;
    if (!resValue) {
        showStatus('请选择分辨率', true);
        return;
    }

    const [width, height] = resValue.split('x').map(Number);
    const fps = parseInt(fpsInput.value) || 30;

    try {
        // 确保先停止任何现有的预览
        console.log('确保先停止任何现有的预览...');
        try {
            await fetch('/api/cameras/stop_preview', {
                method: 'POST'
            });
            // 等待一小段时间，确保连接完全断开
            await new Promise(resolve => setTimeout(resolve, 1000));
        } catch (e) {
            console.log('停止现有预览时出错，可能没有活跃的预览:', e);
        }

        // 打开摄像头
        console.log('正在打开摄像头...');
        const openResponse = await fetch('/api/cameras/open', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify({
                device_path: selectedCamera,
                format: selectedFormat,
                width: width,
                height: height,
                fps: fps
            })
        });

        const openData = await openResponse.json();
        console.log('打开摄像头响应:', openData);

        if (openData.success) {
            // 启动预览
            console.log('正在启动预览...');
            const previewResponse = await fetch('/api/cameras/start_preview', {
                method: 'POST'
            });

            const previewData = await previewResponse.json();
            console.log('启动预览响应:', previewData);

            if (previewData.success) {
                // 生成唯一的客户端ID，避免浏览器缓存
                const clientId = `client-${Math.random().toString(36).substring(2)}-${Date.now()}`;
                console.log('生成客户端ID:', clientId);

                // 清除旧的预览图像
                previewImg.src = 'data:image/gif;base64,R0lGODlhAQABAAAAACH5BAEKAAEALAAAAAABAAEAAAICTAEAOw==';

                // 等待一小段时间，确保旧的连接完全断开
                await new Promise(resolve => setTimeout(resolve, 500));

                // 刷新预览图像，添加摄像头ID参数和客户端ID
                console.log('正在请求MJPEG流...');

                // 创建一个图像加载事件监听器，检测预览是否成功
                const imgLoadPromise = new Promise((resolve, reject) => {
                    // 设置超时，如果10秒内没有加载成功，则认为失败
                    const timeout = setTimeout(() => {
                        reject(new Error('预览加载超时，可能是服务器拒绝了连接'));
                    }, 10000);

                    // 监听图像加载事件
                    const loadHandler = () => {
                        clearTimeout(timeout);
                        previewImg.removeEventListener('load', loadHandler);
                        previewImg.removeEventListener('error', errorHandler);
                        resolve();
                    };

                    // 监听图像加载错误事件
                    const errorHandler = (e) => {
                        clearTimeout(timeout);
                        previewImg.removeEventListener('load', loadHandler);
                        previewImg.removeEventListener('error', errorHandler);
                        reject(new Error('预览加载失败: ' + e.message));
                    };

                    previewImg.addEventListener('load', loadHandler);
                    previewImg.addEventListener('error', errorHandler);
                });

                // 设置图像源
                previewImg.src = `/api/stream?camera_id=${encodeURIComponent(selectedCamera)}&client_id=${clientId}&t=${new Date().getTime()}`;

                try {
                    // 等待图像加载
                    await imgLoadPromise;

                    // 更新状态
                    isPreviewing = true;
                    updateUIState();

                    showStatus('预览已启动');

                    // 立即更新摄像头状态
                    console.log('预览已启动，开始更新摄像头状态...');
                    // 多次尝试更新状态，因为摄像头连接可能需要一些时间
                    setTimeout(loadCameraConnectionStatus, 1000);
                    setTimeout(loadCameraConnectionStatus, 3000);
                    setTimeout(loadCameraConnectionStatus, 5000);
                } catch (error) {
                    console.error('预览加载失败:', error);

                    // 清除预览图像
                    previewImg.src = 'data:image/gif;base64,R0lGODlhAQABAAAAACH5BAEKAAEALAAAAAABAAEAAAICTAEAOw==';

                    // 尝试停止预览，清理连接
                    try {
                        await fetch('/api/cameras/stop_preview', {
                            method: 'POST'
                        });
                        console.log('已尝试停止预览，清理连接');

                        // 等待服务器清理连接
                        await new Promise(resolve => setTimeout(resolve, 3000));

                        // 显示重试按钮
                        showStatus('预览失败，可能是服务器拒绝了连接。请等待几秒后再试。', true);
                    } catch (e) {
                        console.error('停止预览失败:', e);
                    }

                    // 更新状态
                    isPreviewing = false;
                    updateUIState();
                }
            } else {
                showStatus(`启动预览失败: ${previewData.error}`, true);
            }
        } else {
            showStatus(`打开摄像头失败: ${openData.error}`, true);
        }
    } catch (error) {
        showStatus(`启动预览失败: ${error.message}`, true);
    }
}

// 停止预览
async function stopPreview() {
    if (!isPreviewing) return;

    try {
        console.log('正在停止预览...');

        // 先清除预览图像，立即断开MJPEG流连接
        previewImg.src = 'data:image/gif;base64,R0lGODlhAQABAAAAACH5BAEKAAEALAAAAAABAAEAAAICTAEAOw==';

        // 等待一小段时间，确保浏览器断开连接
        await new Promise(resolve => setTimeout(resolve, 1000));

        // 停止预览
        const response = await fetch('/api/cameras/stop_preview', {
            method: 'POST'
        });

        const data = await response.json();
        console.log('停止预览响应:', data);

        if (data.success) {
            // 更新状态
            isPreviewing = false;
            updateUIState();

            showStatus('预览已停止');

            // 立即更新摄像头状态
            setTimeout(loadCameraConnectionStatus, 1000);

            // 等待更长时间，确保服务器完全清理连接
            await new Promise(resolve => setTimeout(resolve, 2000));

            // 关闭设备，确保完全断开连接
            try {
                console.log('正在关闭设备，确保完全断开连接...');
                const closeResponse = await fetch('/api/cameras/close', {
                    method: 'POST'
                });

                const closeData = await closeResponse.json();
                console.log('关闭设备响应:', closeData);
            } catch (e) {
                console.error('关闭设备出错:', e);
            }
        } else {
            showStatus(`停止预览失败: ${data.error}`, true);
        }
    } catch (error) {
        console.error('停止预览出错:', error);

        // 即使出错，也更新UI状态
        isPreviewing = false;
        updateUIState();

        showStatus(`停止预览失败: ${error.message}`, true);
    }
}

// 拍照
async function captureImage() {
    if (!isPreviewing) {
        showStatus('请先开始预览', true);
        return;
    }

    // 检查预览图像是否已加载
    if (previewImg.src === 'data:image/gif;base64,R0lGODlhAQABAAAAACH5BAEKAAEALAAAAAABAAEAAAICTAEAOw==' ||
        !previewImg.complete ||
        !previewImg.naturalWidth) {
        showStatus('预览图像尚未加载，无法拍照。请等待预览加载完成或重新开始预览。', true);
        return;
    }

    try {
        showStatus('正在拍照...');

        const response = await fetch('/api/cameras/capture', {
            method: 'POST'
        });

        const data = await response.json();
        console.log('拍照响应:', data);

        if (data.status === 'success') {
            showStatus('拍照成功');
        } else {
            // 如果拍照失败，可能是预览状态不正确，尝试重新启动预览
            showStatus(`拍照失败: ${data.message}。尝试重新开始预览后再试。`, true);

            // 如果预览状态不正确，更新UI状态
            if (data.message && data.message.includes('摄像头未打开')) {
                isPreviewing = false;
                updateUIState();
            }
        }
    } catch (error) {
        console.error('拍照出错:', error);
        showStatus(`拍照失败: ${error.message}。请尝试重新开始预览。`, true);
    }
}

// 开始录制
async function startRecording() {
    if (isRecording) return;

    if (!isPreviewing) {
        showStatus('请先开始预览', true);
        return;
    }

    try {
        const response = await fetch('/api/cameras/start_recording', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify({
                format: 'mp4',
                encoder: 'h264_rkmpp',
                bitrate: 4000000
            })
        });

        const data = await response.json();

        if (data.success) {
            isRecording = true;
            recordingStartTime = Date.now();
            recordingStatus.style.display = 'block';

            // 更新UI状态
            updateUIState();

            // 启动计时器
            recordingTimer = setInterval(updateRecordingTime, 1000);

            showStatus('录制已开始');
        } else {
            showStatus(`开始录制失败: ${data.error}`, true);
        }
    } catch (error) {
        showStatus(`开始录制失败: ${error.message}`, true);
    }
}

// 停止录制
async function stopRecording() {
    if (!isRecording) return;

    try {
        const response = await fetch('/api/cameras/stop_recording', {
            method: 'POST'
        });

        const data = await response.json();

        if (data.success) {
            isRecording = false;
            recordingStatus.style.display = 'none';

            // 更新UI状态
            updateUIState();

            // 停止计时器
            clearInterval(recordingTimer);

            showStatus('录制已停止');
        } else {
            showStatus(`停止录制失败: ${data.error}`, true);
        }
    } catch (error) {
        showStatus(`停止录制失败: ${error.message}`, true);
    }
}

// 更新录制时间
function updateRecordingTime() {
    if (!isRecording) return;

    const elapsedSeconds = Math.floor((Date.now() - recordingStartTime) / 1000);
    const minutes = Math.floor(elapsedSeconds / 60).toString().padStart(2, '0');
    const seconds = (elapsedSeconds % 60).toString().padStart(2, '0');

    recordingTime.textContent = `${minutes}:${seconds}`;
}





// 显示状态信息
function showStatus(message, isError = false) {
    statusElement.textContent = message;
    statusElement.style.display = 'block';

    if (isError) {
        statusElement.classList.add('error');
    } else {
        statusElement.classList.remove('error');
    }

    // 5秒后自动隐藏
    setTimeout(() => {
        statusElement.style.display = 'none';
    }, 5000);
}

// 加载系统信息
async function loadSystemInfo() {
    try {
        const response = await fetch('/systeminfo');
        const data = await response.json();

        if (data) {
            // 更新系统概览
            hostnameElement.textContent = data.hostname || '--';
            osVersionElement.textContent = data.os_version || '--';
            kernelVersionElement.textContent = data.kernel_version || '--';
            uptimeElement.textContent = data.uptime || '--';
            systemTimeElement.textContent = data.system_time || '--';

            // 更新CPU信息
            cpuInfoElement.textContent = data.cpu_info || '--';

            if (data.cpu_usage !== undefined) {
                cpuUsageElement.textContent = `${data.cpu_usage}%`;
                cpuUsageBarElement.style.width = `${data.cpu_usage}%`;

                // 根据使用率设置颜色
                if (data.cpu_usage > 90) {
                    cpuUsageBarElement.className = 'progress-fill danger';
                } else if (data.cpu_usage > 70) {
                    cpuUsageBarElement.className = 'progress-fill warning';
                } else {
                    cpuUsageBarElement.className = 'progress-fill';
                }
            }

            if (data.cpu_temperature !== undefined) {
                cpuTempElement.textContent = `${data.cpu_temperature}°C`;
            }

            if (data.cpu_frequency !== undefined) {
                cpuFreqElement.textContent = `${data.cpu_frequency} MHz`;
            }

            if (data.load_average) {
                loadAverageElement.textContent = data.load_average.join(', ');
            }

            // 更新GPU信息
            if (data.gpu_usage !== undefined) {
                gpuUsageElement.textContent = `${data.gpu_usage}%`;
                gpuUsageBarElement.style.width = `${data.gpu_usage}%`;

                if (data.gpu_usage > 90) {
                    gpuUsageBarElement.className = 'progress-fill danger';
                } else if (data.gpu_usage > 70) {
                    gpuUsageBarElement.className = 'progress-fill warning';
                } else {
                    gpuUsageBarElement.className = 'progress-fill';
                }
            }

            if (data.gpu_temperature !== undefined) {
                gpuTempElement.textContent = `${data.gpu_temperature}°C`;
            }

            if (data.gpu_memory_usage !== undefined) {
                gpuMemUsageElement.textContent = `${data.gpu_memory_usage}%`;
                gpuMemUsageBarElement.style.width = `${data.gpu_memory_usage}%`;

                if (data.gpu_memory_usage > 90) {
                    gpuMemUsageBarElement.className = 'progress-fill danger';
                } else if (data.gpu_memory_usage > 70) {
                    gpuMemUsageBarElement.className = 'progress-fill warning';
                } else {
                    gpuMemUsageBarElement.className = 'progress-fill';
                }
            }

            if (data.gpu_frequency !== undefined) {
                gpuFreqElement.textContent = `${data.gpu_frequency} MHz`;
            }

            // 更新内存信息
            memTotalElement.textContent = data.memory_total || '--';
            memUsedElement.textContent = data.memory_used || '--';
            memFreeElement.textContent = data.memory_free || '--';

            if (data.memory_usage !== undefined) {
                memUsageElement.textContent = `${data.memory_usage}%`;
                memUsageBarElement.style.width = `${data.memory_usage}%`;

                if (data.memory_usage > 90) {
                    memUsageBarElement.className = 'progress-fill danger';
                } else if (data.memory_usage > 70) {
                    memUsageBarElement.className = 'progress-fill warning';
                } else {
                    memUsageBarElement.className = 'progress-fill';
                }
            }

            // 更新存储信息
            diskTotalElement.textContent = data.disk_total || '--';
            diskUsedElement.textContent = data.disk_used || '--';
            diskFreeElement.textContent = data.disk_free || '--';

            if (data.disk_usage !== undefined) {
                diskUsageElement.textContent = `${data.disk_usage}%`;
                diskUsageBarElement.style.width = `${data.disk_usage}%`;

                if (data.disk_usage > 90) {
                    diskUsageBarElement.className = 'progress-fill danger';
                } else if (data.disk_usage > 70) {
                    diskUsageBarElement.className = 'progress-fill warning';
                } else {
                    diskUsageBarElement.className = 'progress-fill';
                }
            }

            // 更新网络信息
            if (data.network && data.network.length > 0) {
                // 获取第一个网络接口的IP地址
                ipAddressElement.textContent = data.network[0].ip_address || '--';

                // 清空网络接口信息
                networkInterfacesElement.innerHTML = '';

                // 添加网络接口信息
                data.network.forEach(network => {
                    const interfaceDiv = document.createElement('div');
                    interfaceDiv.className = 'network-interface';

                    interfaceDiv.innerHTML = `
                        <div class="info-item">
                            <span class="info-label">${network.interface}:</span>
                            <span class="info-value">${network.ip_address}</span>
                        </div>
                        <div class="info-item">
                            <span class="info-label">接收速率:</span>
                            <span class="info-value">${network.rx_rate} KB/s</span>
                        </div>
                        <div class="info-item">
                            <span class="info-label">发送速率:</span>
                            <span class="info-value">${network.tx_rate} KB/s</span>
                        </div>
                    `;

                    networkInterfacesElement.appendChild(interfaceDiv);
                });
            }

            // 更新WiFi SSID
            wifiSsidElement.textContent = data.wifi_ssid || '--';
        }
    } catch (error) {
        console.error('加载系统信息失败:', error);
    }
}

// 更新时钟
function updateClock() {
    const now = new Date();
    const hours = now.getHours().toString().padStart(2, '0');
    const minutes = now.getMinutes().toString().padStart(2, '0');
    const seconds = now.getSeconds().toString().padStart(2, '0');

    clockElement.textContent = `${hours}:${minutes}:${seconds}`;
}

// 初始化
document.addEventListener('DOMContentLoaded', () => {
    // 初始化标签页
    tabs.forEach(tab => {
        tab.addEventListener('click', () => {
            const tabId = tab.dataset.tab;

            // 切换标签页
            tabs.forEach(t => t.classList.remove('active'));
            tab.classList.add('active');

            // 切换内容
            tabContents.forEach(content => {
                content.classList.remove('active');
            });
            document.getElementById(`${tabId}Tab`).classList.add('active');

            // 如果切换到系统信息标签页，加载系统信息
            if (tabId === 'system') {
                loadSystemInfo();

                // 启动定时器，每5秒更新一次系统信息
                if (systemInfoTimer) {
                    clearInterval(systemInfoTimer);
                }
                systemInfoTimer = setInterval(loadSystemInfo, 5000);
            } else {
                // 停止系统信息更新定时器
                if (systemInfoTimer) {
                    clearInterval(systemInfoTimer);
                    systemInfoTimer = null;
                }
            }
        });
    });



    // 初始化事件监听器
    cameraSelect.addEventListener('change', onCameraChange);
    applySettingsBtn.addEventListener('click', applySettings);
    startPreviewBtn.addEventListener('click', startPreview);
    stopPreviewBtn.addEventListener('click', stopPreview);
    captureBtn.addEventListener('click', captureImage);
    startRecordingBtn.addEventListener('click', startRecording);
    stopRecordingBtn.addEventListener('click', stopRecording);

    // 加载摄像头列表
    loadCameras();

    // 更新时钟
    updateClock();
    setInterval(updateClock, 1000);
});
