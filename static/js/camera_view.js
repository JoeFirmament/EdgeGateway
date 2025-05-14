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
const clockElement = document.getElementById('clock');

// 摄像头状态元素
const connectionStatusElement = document.getElementById('connectionStatus');
const clientIdElement = document.getElementById('clientId');
const currentResolutionElement = document.getElementById('currentResolution');
const currentFpsElement = document.getElementById('currentFps');



// 初始化
document.addEventListener('DOMContentLoaded', () => {
    // 加载摄像头列表
    loadCameras();

    // 初始化时钟
    updateClock();
    setInterval(updateClock, 1000);



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
            updateConnectionStatus(false);
        }
    } catch (error) {
        console.error('加载摄像头连接状态失败:', error);
        updateConnectionStatus(false);
    }
}

// 更新连接状态显示
function updateConnectionStatus(isConnected, data = null) {
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
        // 未连接状态
        connectionStatusElement.textContent = '未连接';
        connectionStatusElement.className = 'status-value disconnected';
        clientIdElement.textContent = '--';
        currentResolutionElement.textContent = '--';
        currentFpsElement.textContent = '--';
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
        await stopPreview();
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
        // 打开摄像头
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

        if (openData.success) {
            // 启动预览
            const previewResponse = await fetch('/api/cameras/start_preview', {
                method: 'POST'
            });

            const previewData = await previewResponse.json();

            if (previewData.success) {
                // 生成唯一的客户端ID，避免浏览器缓存
                const clientId = `client-${Math.random().toString(36).substring(2)}-${Date.now()}`;

                // 刷新预览图像，添加摄像头ID参数和客户端ID
                previewImg.src = `/api/stream?camera_id=${encodeURIComponent(selectedCamera)}&client_id=${clientId}&t=${new Date().getTime()}`;

                // 更新状态
                isPreviewing = true;
                updateUIState();

                showStatus('预览已启动');

                // 立即更新摄像头状态
                setTimeout(loadCameraConnectionStatus, 1000);
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
        // 停止预览
        const response = await fetch('/api/cameras/stop_preview', {
            method: 'POST'
        });

        const data = await response.json();

        if (data.success) {
            // 清除预览图像
            previewImg.src = 'data:image/gif;base64,R0lGODlhAQABAAAAACH5BAEKAAEALAAAAAABAAEAAAICTAEAOw==';

            // 更新状态
            isPreviewing = false;
            updateUIState();

            showStatus('预览已停止');

            // 立即更新摄像头状态
            setTimeout(loadCameraConnectionStatus, 1000);
        } else {
            showStatus(`停止预览失败: ${data.error}`, true);
        }
    } catch (error) {
        showStatus(`停止预览失败: ${error.message}`, true);
    }
}

// 拍照
async function captureImage() {
    if (!isPreviewing) {
        showStatus('请先开始预览', true);
        return;
    }

    try {
        const response = await fetch('/api/cameras/capture', {
            method: 'POST'
        });

        const data = await response.json();

        if (data.status === 'success') {
            showStatus('拍照成功');
        } else {
            showStatus(`拍照失败: ${data.message}`, true);
        }
    } catch (error) {
        showStatus(`拍照失败: ${error.message}`, true);
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
