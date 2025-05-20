# 摄像头服务器项目 C++ 编码风格指南

## 1. 命名约定

### 1.1 文件命名
- 头文件：使用 `snake_case`，例如 `rest_handler.h`
- 源文件：使用 `snake_case`，例如 `rest_handler.cpp`

### 1.2 类型命名
- 类名：使用 `PascalCase`，例如 `ApiServer`
- 结构体：使用 `PascalCase`，例如 `WebServerConfig`
- 枚举类：使用 `PascalCase`，例如 `ApiServerStatus::State`

### 1.3 变量命名
- 成员变量：使用 `snake_case` 并以下划线结尾，例如 `is_initialized_`
- 局部变量：使用 `snake_case`，例如 `request_count`
- 常量：使用 `UPPER_SNAKE_CASE`，例如 `MAX_CONNECTIONS`

### 1.4 函数命名
- 成员函数：使用 `camelCase`，例如 `getStatus()`
- 静态函数：使用 `camelCase`，例如 `getInstance()`

## 2. 命名空间

### 2.1 命名空间结构
- 主命名空间：`cam_server`
- 子命名空间：`api`、`camera`、`system` 等
- 使用 `using namespace std;` 简化标准库类型使用

```cpp
namespace cam_server::api {
    using namespace std;
    // 代码实现
}
```

## 3. 类设计

### 3.1 单例模式
- 使用局部静态变量实现线程安全的单例
- 私有构造函数
- 删除拷贝构造和赋值运算符

```cpp
class ApiServer {
public:
    static ApiServer& getInstance() {
        static ApiServer instance;
        return instance;
    }

private:
    ApiServer() = default;
    ~ApiServer() = default;
    ApiServer(const ApiServer&) = delete;
    ApiServer& operator=(const ApiServer&) = delete;
};
```

### 3.2 资源管理
- 优先使用智能指针
- `unique_ptr` 用于独占资源
- `shared_ptr` 用于共享资源

```cpp
unique_ptr<WebServer> web_server_;
shared_ptr<RestHandler> rest_handler_;
```

## 4. 类型安全

### 4.1 类型声明
- 使用 `enum class` 替代传统枚举
- 使用 `using` 类型别名
- 使用 `auto` 简化复杂类型声明

```cpp
enum class ApiServerStatus {
    STOPPED,
    RUNNING,
    ERROR
};

using RouteHandler = function<void(const HttpRequest&, HttpResponse&)>;
```

## 5. 异常处理

### 5.1 异常捕获
- 捕获 `const exception&`
- 记录详细错误日志
- 提供友好的错误响应

```cpp
try {
    // 业务逻辑
} catch (const exception& e) {
    LOG_ERROR("操作失败: " + string(e.what()), "模块名");
    // 返回错误响应
}
```

## 6. 并发与线程安全

### 6.1 状态同步
- 使用 `mutex` 保护共享状态
- 使用 `atomic` 变量管理简单状态
- 使用 `lock_guard` 进行RAII锁管理

```cpp
mutable mutex status_mutex_;
atomic<bool> is_running_{false};

void updateStatus() {
    lock_guard<mutex> lock(status_mutex_);
    // 安全更新状态
}
```

## 7. 函数设计

### 7.1 参数传递
- 复杂类型使用 `const` 引用
- 简单类型直接传值
- 对于可选参数，提供默认值

```cpp
string generateClientId(const string& client_id_hint = "");
```

## 8. 注释与文档

### 8.1 文档注释
- 使用 Doxygen 风格注释
- 描述函数/类的目的
- 解释参数和返回值
- 提供使用示例（可选）

```cpp
/**
 * @brief 生成唯一客户端ID
 * @param client_id_hint 可选的客户端ID前缀
 * @return 生成的唯一客户端ID
 */
```

## 9. 性能与优化

### 9.1 编译优化
- 使用 `-O2` 或 `-O3` 优化级别
- 启用 Link Time Optimization (LTO)
- 使用现代编译器特性

### 9.2 内存管理
- 优先使用栈分配
- 避免不必要的内存分配
- 使用引用和指针传递大型对象

## 10. 最佳实践

1. 遵循 SOLID 设计原则
2. 保持函数短小，单一职责
3. 优先使用标准库实现
4. 避免全局变量
5. 使用 `const` 保证不可变性
6. 优先使用编译期检查

## 附录：代码示例

```cpp
class RestHandler {
public:
    // 单例模式
    static RestHandler& getInstance() {
        static RestHandler instance;
        return instance;
    }

    // 线程安全的路由注册
    bool registerRoute(const string& method, const string& path, RouteHandler handler) {
        lock_guard<mutex> lock(routes_mutex_);
        RouteKey key{method, path};
        routes_[key] = handler;
        return true;
    }

private:
    // 私有构造函数
    RestHandler() = default;
    ~RestHandler() = default;
    RestHandler(const RestHandler&) = delete;
    RestHandler& operator=(const RestHandler&) = delete;

    // 线程安全的成员
    mutex routes_mutex_;
    unordered_map<RouteKey, RouteHandler> routes_;
};
```

## 结语

本指南旨在提供一个灵活且高效的编码风格指导。随着项目的发展，指南也将不断演进和完善。 