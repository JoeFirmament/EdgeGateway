/**
 * 统一导航栏管理器
 * 负责加载导航栏组件并设置当前页面的激活状态
 */
class NavigationManager {
    constructor() {
        this.currentPage = this.detectCurrentPage();
        this.navigationLoaded = false;
    }

    /**
     * 初始化导航栏
     * @param {string} containerId - 导航栏容器的ID，默认为 'navigation-container'
     */
    async init(containerId = 'navigation-container') {
        try {
            await this.loadNavigation(containerId);
            this.setActivePage(this.currentPage);
            this.navigationLoaded = true;
            console.log('🧭 导航栏加载完成，当前页面:', this.currentPage);
        } catch (error) {
            console.error('❌ 导航栏加载失败:', error);
        }
    }

    /**
     * 加载导航栏HTML
     * @param {string} containerId - 容器ID
     */
    async loadNavigation(containerId) {
        const container = document.getElementById(containerId);
        if (!container) {
            throw new Error(`找不到导航栏容器: ${containerId}`);
        }

        try {
            const response = await fetch('/static/components/navigation.html');
            if (!response.ok) {
                throw new Error(`HTTP ${response.status}: ${response.statusText}`);
            }
            
            const navigationHtml = await response.text();
            container.innerHTML = navigationHtml;
        } catch (error) {
            console.error('加载导航栏组件失败:', error);
            // 提供降级方案
            container.innerHTML = this.getFallbackNavigation();
        }
    }

    /**
     * 检测当前页面
     * @returns {string} 页面标识
     */
    detectCurrentPage() {
        const path = window.location.pathname;
        
        // 页面路径映射
        const pageMap = {
            '/': 'home',
            '/index.html': 'home',
            '/video_recording.html': 'video_recording',
            '/frame_extraction.html': 'frame_extraction',
            '/photo_capture.html': 'photo_capture',
            '/system_info.html': 'system_info',
            '/serial_info.html': 'serial_info'
        };

        // 检查完整路径
        if (pageMap[path]) {
            return pageMap[path];
        }

        // 检查文件名
        const filename = path.split('/').pop();
        if (pageMap['/' + filename]) {
            return pageMap['/' + filename].replace('/', '');
        }

        // 默认返回home
        return 'home';
    }

    /**
     * 设置当前激活的页面
     * @param {string} pageName - 页面名称
     */
    setActivePage(pageName) {
        // 移除所有激活状态
        const navTabs = document.querySelectorAll('.nav-tab');
        navTabs.forEach(tab => {
            tab.classList.remove('active');
        });

        // 设置当前页面为激活状态
        const activeTab = document.querySelector(`.nav-tab[data-page="${pageName}"]`);
        if (activeTab) {
            activeTab.classList.add('active');
        } else {
            console.warn('未找到对应的导航标签:', pageName);
            // 默认激活首页
            const homeTab = document.querySelector('.nav-tab[data-page="home"]');
            if (homeTab) {
                homeTab.classList.add('active');
            }
        }
    }

    /**
     * 获取降级导航栏HTML（当组件加载失败时使用）
     * @returns {string} 导航栏HTML
     */
    getFallbackNavigation() {
        return `
            <nav class="top-navigation">
                <div class="nav-tabs">
                    <a href="/" class="nav-tab" data-page="home">🏠 主页</a>
                    <a href="/video_recording.html" class="nav-tab" data-page="video_recording">🎬 录制</a>
                    <a href="/frame_extraction.html" class="nav-tab" data-page="frame_extraction">🖼️ 帧提取</a>
                    <a href="/photo_capture.html" class="nav-tab" data-page="photo_capture">📸 拍照</a>
                    <a href="/system_info.html" class="nav-tab" data-page="system_info">🖥️ 系统</a>
                    <a href="/serial_info.html" class="nav-tab" data-page="serial_info">🔌 串口</a>
                </div>
            </nav>
        `;
    }

    /**
     * 更新导航栏（用于动态添加或修改导航项）
     * @param {Array} navItems - 导航项数组
     */
    updateNavigation(navItems) {
        const navTabsContainer = document.querySelector('.nav-tabs');
        if (!navTabsContainer) {
            console.error('找不到导航标签容器');
            return;
        }

        // 清空现有导航项
        navTabsContainer.innerHTML = '';

        // 添加新的导航项
        navItems.forEach(item => {
            const navTab = document.createElement('a');
            navTab.href = item.href;
            navTab.className = 'nav-tab';
            navTab.setAttribute('data-page', item.page);
            navTab.innerHTML = `${item.icon} ${item.title}`;
            navTabsContainer.appendChild(navTab);
        });

        // 重新设置激活状态
        this.setActivePage(this.currentPage);
    }

    /**
     * 获取当前页面标识
     * @returns {string} 当前页面标识
     */
    getCurrentPage() {
        return this.currentPage;
    }

    /**
     * 检查导航栏是否已加载
     * @returns {boolean} 是否已加载
     */
    isLoaded() {
        return this.navigationLoaded;
    }
}

// 创建全局导航管理器实例
window.navigationManager = new NavigationManager();

// 提供便捷的初始化函数
window.initNavigation = function(containerId = 'navigation-container') {
    return window.navigationManager.init(containerId);
};

// 页面加载完成后自动初始化（如果存在导航容器）
document.addEventListener('DOMContentLoaded', function() {
    const navigationContainer = document.getElementById('navigation-container');
    if (navigationContainer) {
        window.initNavigation();
    }
});

// 导出给其他模块使用
if (typeof module !== 'undefined' && module.exports) {
    module.exports = NavigationManager;
}
