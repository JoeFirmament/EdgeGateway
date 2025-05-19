document.addEventListener('DOMContentLoaded', function() {
    // 获取DOM元素
    const startStreamBtn = document.getElementById('start-stream');
    const stopStreamBtn = document.getElementById('stop-stream');
    const openCameraBtn = document.getElementById('open-camera');
    const closeCameraBtn = document.getElementById('close-camera');
    const captureImageBtn = document.getElementById('capture-image');
    const recordVideoBtn = document.getElementById('record-video');
    const stopRecordingBtn = document.getElementById('stop-recording');
    const cameraSettingsForm = document.getElementById('camera-settings');
    const streamImg = document.getElementById('stream');
    
    // 开始流
    startStreamBtn.addEventListener('click', function() {
        fetch('/api/stream/start', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            }
        })
        .then(response => response.json())
        .then(data => {
            if (data.status === 'success') {
                // 刷新流
                streamImg.src = '/api/stream/mjpeg?' + new Date().getTime();
                showMessage('成功开始视频流', 'success');
            } else {
                showMessage('无法开始视频流: ' + data.message, 'error');
            }
        })
        .catch(error => {
            showMessage('请求错误: ' + error, 'error');
        });
    });
    
    // 停止流
    stopStreamBtn.addEventListener('click', function() {
        fetch('/api/stream/stop', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            }
        })
        .then(response => response.json())
        .then(data => {
            if (data.status === 'success') {
                // 清除流
                streamImg.src = '';
                showMessage('成功停止视频流', 'success');
            } else {
                showMessage('无法停止视频流: ' + data.message, 'error');
            }
        })
        .catch(error => {
            showMessage('请求错误: ' + error, 'error');
        });
    });
    
    // 打开摄像头
    openCameraBtn.addEventListener('click', function() {
        const resolution = document.getElementById('resolution').value;
        const [width, height] = resolution.split('x');
        const fps = document.getElementById('fps').value;
        const format = document.getElementById('format').value;
        
        fetch('/api/camera/control', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify({
                action: 'open',
                width: parseInt(width),
                height: parseInt(height),
                fps: parseInt(fps),
                format: format
            })
        })
        .then(response => response.json())
        .then(data => {
            if (data.status === 'success') {
                showMessage('成功打开摄像头', 'success');
            } else {
                showMessage('无法打开摄像头: ' + data.message, 'error');
            }
        })
        .catch(error => {
            showMessage('请求错误: ' + error, 'error');
        });
    });
    
    // 关闭摄像头
    closeCameraBtn.addEventListener('click', function() {
        fetch('/api/camera/control', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify({
                action: 'close'
            })
        })
        .then(response => response.json())
        .then(data => {
            if (data.status === 'success') {
                // 清除流
                streamImg.src = '';
                showMessage('成功关闭摄像头', 'success');
            } else {
                showMessage('无法关闭摄像头: ' + data.message, 'error');
            }
        })
        .catch(error => {
            showMessage('请求错误: ' + error, 'error');
        });
    });
    
    // 拍照
    captureImageBtn.addEventListener('click', function() {
        fetch('/api/camera/capture', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            }
        })
        .then(response => response.json())
        .then(data => {
            if (data.status === 'success') {
                showMessage('成功拍照: ' + data.filename, 'success');
                
                // 如果返回了图片URL，可以在新窗口中打开
                if (data.url) {
                    window.open(data.url, '_blank');
                }
            } else {
                showMessage('无法拍照: ' + data.message, 'error');
            }
        })
        .catch(error => {
            showMessage('请求错误: ' + error, 'error');
        });
    });
    
    // 开始录制视频
    recordVideoBtn.addEventListener('click', function() {
        fetch('/api/camera/record', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify({
                action: 'start'
            })
        })
        .then(response => response.json())
        .then(data => {
            if (data.status === 'success') {
                showMessage('开始录制视频', 'success');
                recordVideoBtn.disabled = true;
                stopRecordingBtn.disabled = false;
            } else {
                showMessage('无法开始录制视频: ' + data.message, 'error');
            }
        })
        .catch(error => {
            showMessage('请求错误: ' + error, 'error');
        });
    });
    
    // 停止录制视频
    stopRecordingBtn.addEventListener('click', function() {
        fetch('/api/camera/record', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify({
                action: 'stop'
            })
        })
        .then(response => response.json())
        .then(data => {
            if (data.status === 'success') {
                showMessage('停止录制视频: ' + data.filename, 'success');
                recordVideoBtn.disabled = false;
                stopRecordingBtn.disabled = true;
            } else {
                showMessage('无法停止录制视频: ' + data.message, 'error');
            }
        })
        .catch(error => {
            showMessage('请求错误: ' + error, 'error');
        });
    });
    
    // 应用摄像头设置
    cameraSettingsForm.addEventListener('submit', function(e) {
        e.preventDefault();
        
        const resolution = document.getElementById('resolution').value;
        const [width, height] = resolution.split('x');
        const fps = document.getElementById('fps').value;
        const format = document.getElementById('format').value;
        
        fetch('/api/camera/control', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify({
                action: 'set_params',
                width: parseInt(width),
                height: parseInt(height),
                fps: parseInt(fps),
                format: format
            })
        })
        .then(response => response.json())
        .then(data => {
            if (data.status === 'success') {
                showMessage('成功应用摄像头设置', 'success');
                
                // 刷新流
                if (streamImg.src) {
                    streamImg.src = '/api/stream/mjpeg?' + new Date().getTime();
                }
            } else {
                showMessage('无法应用摄像头设置: ' + data.message, 'error');
            }
        })
        .catch(error => {
            showMessage('请求错误: ' + error, 'error');
        });
    });
    
    // 获取摄像头信息
    function getCameraInfo() {
        fetch('/api/camera/info')
        .then(response => response.json())
        .then(data => {
            if (data.status === 'success') {
                // 更新UI以反映当前摄像头状态
                updateCameraUI(data.camera, data.params);
            }
        })
        .catch(error => {
            console.error('获取摄像头信息失败:', error);
        });
    }
    
    // 更新摄像头UI
    function updateCameraUI(camera, params) {
        // 更新按钮状态
        openCameraBtn.disabled = camera.open;
        closeCameraBtn.disabled = !camera.open;
        startStreamBtn.disabled = !camera.open || camera.capturing;
        stopStreamBtn.disabled = !camera.open || !camera.capturing;
        captureImageBtn.disabled = !camera.open;
        recordVideoBtn.disabled = !camera.open;
        stopRecordingBtn.disabled = true; // 默认禁用，只有在录制时才启用
        
        // 更新设置表单
        if (params) {
            const resolutionSelect = document.getElementById('resolution');
            const fpsSelect = document.getElementById('fps');
            const formatSelect = document.getElementById('format');
            
            // 设置分辨率
            const resolution = params.width + 'x' + params.height;
            for (let i = 0; i < resolutionSelect.options.length; i++) {
                if (resolutionSelect.options[i].value === resolution) {
                    resolutionSelect.selectedIndex = i;
                    break;
                }
            }
            
            // 设置帧率
            for (let i = 0; i < fpsSelect.options.length; i++) {
                if (parseInt(fpsSelect.options[i].value) === params.fps) {
                    fpsSelect.selectedIndex = i;
                    break;
                }
            }
            
            // 设置格式
            for (let i = 0; i < formatSelect.options.length; i++) {
                if (formatSelect.options[i].value === params.format) {
                    formatSelect.selectedIndex = i;
                    break;
                }
            }
        }
        
        // 如果摄像头已打开且正在捕获，显示流
        if (camera.open && camera.capturing) {
            streamImg.src = '/api/stream/mjpeg?' + new Date().getTime();
        } else {
            streamImg.src = '';
        }
    }
    
    // 显示消息
    function showMessage(message, type) {
        // 创建消息元素
        const messageDiv = document.createElement('div');
        messageDiv.className = 'message ' + type;
        messageDiv.textContent = message;
        
        // 添加到页面
        document.body.appendChild(messageDiv);
        
        // 3秒后自动移除
        setTimeout(() => {
            messageDiv.classList.add('fade-out');
            setTimeout(() => {
                document.body.removeChild(messageDiv);
            }, 500);
        }, 3000);
    }
    
    // 初始化：获取摄像头信息
    getCameraInfo();
    
    // 定期更新摄像头信息
    setInterval(getCameraInfo, 5000);
});
