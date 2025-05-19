document.addEventListener('DOMContentLoaded', function() {
    // 获取DOM元素
    const systemOverviewEl = document.getElementById('system-overview');
    const cpuUsageEl = document.getElementById('cpu-usage');
    const memoryUsageEl = document.getElementById('memory-usage');
    const storageUsageEl = document.getElementById('storage-usage');
    const networkStatusEl = document.getElementById('network-status');
    
    // 格式化字节大小
    function formatBytes(bytes, decimals = 2) {
        if (bytes === 0) return '0 Bytes';
        
        const k = 1024;
        const dm = decimals < 0 ? 0 : decimals;
        const sizes = ['Bytes', 'KB', 'MB', 'GB', 'TB', 'PB', 'EB', 'ZB', 'YB'];
        
        const i = Math.floor(Math.log(bytes) / Math.log(k));
        
        return parseFloat((bytes / Math.pow(k, i)).toFixed(dm)) + ' ' + sizes[i];
    }
    
    // 格式化速率
    function formatRate(bytesPerSec) {
        return formatBytes(bytesPerSec) + '/s';
    }
    
    // 创建进度条
    function createProgressBar(percent, color = 'primary') {
        const progressBar = document.createElement('div');
        progressBar.className = 'progress';
        
        const progressBarInner = document.createElement('div');
        progressBarInner.className = 'progress-bar progress-bar-' + color;
        progressBarInner.style.width = percent + '%';
        progressBarInner.textContent = percent.toFixed(1) + '%';
        
        progressBar.appendChild(progressBarInner);
        return progressBar;
    }
    
    // 更新系统概览
    function updateSystemOverview(data) {
        systemOverviewEl.innerHTML = '';
        
        const table = document.createElement('table');
        table.className = 'info-table';
        
        // 主机名
        let row = table.insertRow();
        row.insertCell(0).textContent = '主机名';
        row.insertCell(1).textContent = data.hostname;
        
        // 操作系统
        row = table.insertRow();
        row.insertCell(0).textContent = '操作系统';
        row.insertCell(1).textContent = data.os;
        
        // 内核版本
        row = table.insertRow();
        row.insertCell(0).textContent = '内核版本';
        row.insertCell(1).textContent = data.kernel;
        
        // 运行时间
        row = table.insertRow();
        row.insertCell(0).textContent = '运行时间';
        row.insertCell(1).textContent = data.uptime;
        
        // 系统时间
        row = table.insertRow();
        row.insertCell(0).textContent = '系统时间';
        row.insertCell(1).textContent = data.system_time;
        
        systemOverviewEl.appendChild(table);
    }
    
    // 更新CPU使用率
    function updateCpuUsage(data) {
        cpuUsageEl.innerHTML = '';
        
        // 总体CPU使用率
        const cpuUsageTitle = document.createElement('h3');
        cpuUsageTitle.textContent = '总体使用率: ' + data.usage.toFixed(1) + '%';
        cpuUsageEl.appendChild(cpuUsageTitle);
        
        // CPU使用率进度条
        cpuUsageEl.appendChild(createProgressBar(data.usage, getCpuUsageColor(data.usage)));
        
        // CPU温度
        if (data.temperature > 0) {
            const cpuTempTitle = document.createElement('h3');
            cpuTempTitle.textContent = 'CPU温度: ' + data.temperature.toFixed(1) + '°C';
            cpuUsageEl.appendChild(cpuTempTitle);
            
            // 温度进度条
            const tempPercent = (data.temperature / 100) * 100; // 假设100°C是最高温度
            cpuUsageEl.appendChild(createProgressBar(tempPercent, getCpuTempColor(data.temperature)));
        }
        
        // 核心使用率
        if (data.core_usage && data.core_usage.length > 0) {
            const coresTitle = document.createElement('h3');
            coresTitle.textContent = '核心使用率';
            cpuUsageEl.appendChild(coresTitle);
            
            const coresContainer = document.createElement('div');
            coresContainer.className = 'cores-container';
            
            for (let i = 0; i < data.core_usage.length; i++) {
                const coreUsage = data.core_usage[i];
                
                const coreDiv = document.createElement('div');
                coreDiv.className = 'core-item';
                
                const coreTitle = document.createElement('div');
                coreTitle.className = 'core-title';
                coreTitle.textContent = '核心 ' + i + ': ' + coreUsage.toFixed(1) + '%';
                
                coreDiv.appendChild(coreTitle);
                coreDiv.appendChild(createProgressBar(coreUsage, getCpuUsageColor(coreUsage)));
                
                coresContainer.appendChild(coreDiv);
            }
            
            cpuUsageEl.appendChild(coresContainer);
        }
    }
    
    // 更新内存使用
    function updateMemoryUsage(data) {
        memoryUsageEl.innerHTML = '';
        
        // 内存使用率标题
        const memoryTitle = document.createElement('h3');
        memoryTitle.textContent = '内存使用率: ' + data.usage.toFixed(1) + '%';
        memoryUsageEl.appendChild(memoryTitle);
        
        // 内存使用率进度条
        memoryUsageEl.appendChild(createProgressBar(data.usage, getMemoryUsageColor(data.usage)));
        
        // 内存详情
        const memoryDetails = document.createElement('div');
        memoryDetails.className = 'memory-details';
        
        const totalMemory = document.createElement('div');
        totalMemory.textContent = '总内存: ' + formatBytes(data.total);
        
        const usedMemory = document.createElement('div');
        usedMemory.textContent = '已用: ' + formatBytes(data.used);
        
        const freeMemory = document.createElement('div');
        freeMemory.textContent = '空闲: ' + formatBytes(data.free);
        
        memoryDetails.appendChild(totalMemory);
        memoryDetails.appendChild(usedMemory);
        memoryDetails.appendChild(freeMemory);
        
        memoryUsageEl.appendChild(memoryDetails);
    }
    
    // 更新存储空间
    function updateStorageUsage(data) {
        storageUsageEl.innerHTML = '';
        
        if (!data || data.length === 0) {
            storageUsageEl.textContent = '无存储信息';
            return;
        }
        
        // 为每个存储创建一个卡片
        for (const storage of data) {
            const storageCard = document.createElement('div');
            storageCard.className = 'storage-card';
            
            const storageTitle = document.createElement('h3');
            storageTitle.textContent = storage.mount;
            storageCard.appendChild(storageTitle);
            
            // 使用率标题
            const usageTitle = document.createElement('div');
            usageTitle.textContent = '使用率: ' + storage.usage.toFixed(1) + '%';
            storageCard.appendChild(usageTitle);
            
            // 使用率进度条
            storageCard.appendChild(createProgressBar(storage.usage, getStorageUsageColor(storage.usage)));
            
            // 存储详情
            const storageDetails = document.createElement('div');
            storageDetails.className = 'storage-details';
            
            const totalStorage = document.createElement('div');
            totalStorage.textContent = '总容量: ' + formatBytes(storage.total);
            
            const usedStorage = document.createElement('div');
            usedStorage.textContent = '已用: ' + formatBytes(storage.used);
            
            const freeStorage = document.createElement('div');
            freeStorage.textContent = '空闲: ' + formatBytes(storage.free);
            
            storageDetails.appendChild(totalStorage);
            storageDetails.appendChild(usedStorage);
            storageDetails.appendChild(freeStorage);
            
            storageCard.appendChild(storageDetails);
            
            storageUsageEl.appendChild(storageCard);
        }
    }
    
    // 更新网络状态
    function updateNetworkStatus(data) {
        networkStatusEl.innerHTML = '';
        
        if (!data || data.length === 0) {
            networkStatusEl.textContent = '无网络信息';
            return;
        }
        
        // 为每个网络接口创建一个卡片
        for (const network of data) {
            const networkCard = document.createElement('div');
            networkCard.className = 'network-card';
            
            const networkTitle = document.createElement('h3');
            networkTitle.textContent = network.interface;
            networkCard.appendChild(networkTitle);
            
            // IP地址
            if (network.ip) {
                const ipAddress = document.createElement('div');
                ipAddress.textContent = 'IP地址: ' + network.ip;
                networkCard.appendChild(ipAddress);
            }
            
            // 网络流量
            const trafficTitle = document.createElement('h4');
            trafficTitle.textContent = '网络流量';
            networkCard.appendChild(trafficTitle);
            
            const trafficDetails = document.createElement('div');
            trafficDetails.className = 'traffic-details';
            
            const rxTraffic = document.createElement('div');
            rxTraffic.textContent = '下载: ' + formatRate(network.rx_rate);
            
            const txTraffic = document.createElement('div');
            txTraffic.textContent = '上传: ' + formatRate(network.tx_rate);
            
            const totalRx = document.createElement('div');
            totalRx.textContent = '总下载: ' + formatBytes(network.rx_bytes);
            
            const totalTx = document.createElement('div');
            totalTx.textContent = '总上传: ' + formatBytes(network.tx_bytes);
            
            trafficDetails.appendChild(rxTraffic);
            trafficDetails.appendChild(txTraffic);
            trafficDetails.appendChild(totalRx);
            trafficDetails.appendChild(totalTx);
            
            networkCard.appendChild(trafficDetails);
            
            networkStatusEl.appendChild(networkCard);
        }
    }
    
    // 获取CPU使用率颜色
    function getCpuUsageColor(usage) {
        if (usage < 60) return 'success';
        if (usage < 85) return 'warning';
        return 'danger';
    }
    
    // 获取CPU温度颜色
    function getCpuTempColor(temp) {
        if (temp < 50) return 'success';
        if (temp < 70) return 'warning';
        return 'danger';
    }
    
    // 获取内存使用率颜色
    function getMemoryUsageColor(usage) {
        if (usage < 70) return 'success';
        if (usage < 90) return 'warning';
        return 'danger';
    }
    
    // 获取存储使用率颜色
    function getStorageUsageColor(usage) {
        if (usage < 75) return 'success';
        if (usage < 90) return 'warning';
        return 'danger';
    }
    
    // 获取系统信息
    function getSystemInfo() {
        fetch('/api/system/info')
        .then(response => response.json())
        .then(data => {
            if (data.status === 'success') {
                // 更新UI
                updateSystemOverview({
                    hostname: data.system.hostname,
                    os: data.system.os,
                    kernel: data.system.kernel,
                    uptime: data.system.uptime,
                    system_time: data.system.system_time || new Date().toLocaleString()
                });
                
                updateCpuUsage(data.cpu);
                updateMemoryUsage(data.memory);
                updateStorageUsage(data.storage);
                updateNetworkStatus(data.network);
            } else {
                console.error('获取系统信息失败:', data.message);
            }
        })
        .catch(error => {
            console.error('请求错误:', error);
        });
    }
    
    // 初始化：获取系统信息
    getSystemInfo();
    
    // 定期更新系统信息
    setInterval(getSystemInfo, 3000);
});
