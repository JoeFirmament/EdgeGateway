#include <iostream>
#include <mutex>
#include <unordered_set>

// 包含Crow头文件
#include "third_party/crow/crow.h"

int main() {
    crow::SimpleApp app;

    std::mutex mtx;
    std::unordered_set<crow::websocket::connection*> users;

    // 创建WebSocket路由
    CROW_WEBSOCKET_ROUTE(app, "/ws")
      .onopen([&](crow::websocket::connection& conn) {
          std::cout << "=== WebSocket连接打开 ===" << std::endl;
          std::cout << "远程IP: " << conn.get_remote_ip() << std::endl;
          std::lock_guard<std::mutex> _(mtx);
          users.insert(&conn);
          std::cout << "当前连接数: " << users.size() << std::endl;
      })
      .onclose([&](crow::websocket::connection& conn, const std::string& reason, uint16_t code) {
          std::cout << "=== WebSocket连接关闭 ===" << std::endl;
          std::cout << "原因: " << reason << ", 代码: " << code << std::endl;
          std::lock_guard<std::mutex> _(mtx);
          users.erase(&conn);
          std::cout << "当前连接数: " << users.size() << std::endl;
      })
      .onmessage([&](crow::websocket::connection& conn, const std::string& data, bool is_binary) {
          std::cout << "=== 收到WebSocket消息 ===" << std::endl;
          std::cout << "数据: " << data << std::endl;
          std::cout << "是否二进制: " << (is_binary ? "是" : "否") << std::endl;
          
          // 回显消息给发送者
          conn.send_text("Echo: " + data);
          
          // 广播给所有用户
          std::lock_guard<std::mutex> _(mtx);
          for (auto u : users) {
              if (u != &conn) {  // 不发送给自己
                  if (is_binary)
                      u->send_binary(data);
                  else
                      u->send_text("Broadcast: " + data);
              }
          }
      });

    // 添加一个简单的HTTP路由用于测试
    CROW_ROUTE(app, "/")
    ([]{
        return "WebSocket测试服务器运行中！<br>连接到 ws://localhost:8080/ws 进行测试";
    });

    std::cout << "启动WebSocket测试服务器..." << std::endl;
    std::cout << "HTTP: http://localhost:8080/" << std::endl;
    std::cout << "WebSocket: ws://localhost:8080/ws" << std::endl;
    std::cout << "按 Ctrl+C 停止服务器" << std::endl;

    // 启动服务器
    app.port(8080).multithreaded().run();

    return 0;
}
