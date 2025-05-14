# 已知问题和解决方案

## 静态文件路径问题

### 问题描述
当服务器使用相对路径（如 `"static_files_dir": "static"`）来指定静态文件目录时，无法正确找到静态文件。这是因为服务器运行时的工作目录与配置文件中指定的静态文件目录路径不匹配。

当服务器在 `/home/orangepi/Qworkspace/cam_server_cpp/build/bin` 目录下运行时，它会尝试在 `/home/orangepi/Qworkspace/cam_server_cpp/build/bin/static` 目录下查找文件，而不是在 `/home/orangepi/Qworkspace/cam_server_cpp/static` 目录下。

### 临时解决方案
使用绝对路径来指定静态文件目录，例如：
```json
"static_files_dir": "/home/orangepi/Qworkspace/cam_server_cpp/static"
```

或者在代码中硬编码绝对路径：
```cpp
std::string file_path = "/home/orangepi/Qworkspace/cam_server_cpp/static/index.html";
```

### 长期解决方案
1. **使用相对于可执行文件的路径**：
   - 在程序启动时，获取可执行文件的路径
   - 基于可执行文件的路径计算静态文件目录的路径

   ```cpp
   std::string getExecutablePath() {
       char buffer[PATH_MAX];
       ssize_t len = readlink("/proc/self/exe", buffer, sizeof(buffer) - 1);
       if (len != -1) {
           buffer[len] = '\0';
           return std::string(buffer);
       }
       return "";
   }

   std::string getExecutableDir() {
       std::string path = getExecutablePath();
       size_t pos = path.find_last_of('/');
       if (pos != std::string::npos) {
           return path.substr(0, pos);
       }
       return "";
   }

   // 在初始化时
   std::string exe_dir = getExecutableDir();
   std::string static_dir = exe_dir + "/../static";  // 根据项目结构调整路径
   ```

2. **使用环境变量**：
   - 通过环境变量指定静态文件目录的路径
   - 在程序启动时读取环境变量

   ```cpp
   const char* static_dir_env = std::getenv("CAM_SERVER_STATIC_DIR");
   std::string static_dir = static_dir_env ? static_dir_env : "static";  // 默认值为相对路径
   ```

3. **使用命令行参数**：
   - 通过命令行参数指定静态文件目录的路径
   - 在解析命令行参数时设置静态文件目录

   ```cpp
   // 在parse_args函数中
   static struct option long_options[] = {
       // ...
       {"static-dir", required_argument, 0, 's'},
       // ...
   };

   // 解析参数
   case 's':
       static_files_dir = optarg;
       break;
   ```

4. **使用工作目录**：
   - 在启动服务器之前，切换到正确的工作目录
   - 这样相对路径就能正确解析

   ```cpp
   // 在main函数开始处
   chdir("/home/orangepi/Qworkspace/cam_server_cpp");
   ```

### 推荐方案
结合使用相对于可执行文件的路径和命令行参数：
1. 默认使用相对于可执行文件的路径
2. 允许通过命令行参数覆盖默认路径
3. 在配置文件中记录最终使用的路径

这样既保持了灵活性，又避免了硬编码绝对路径的问题。
