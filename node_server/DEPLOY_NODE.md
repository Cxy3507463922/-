# Node.js 版部署指南 (HTML/JS/CSS)

这是使用 Node.js 重写的版本，前端完全采用 HTML/JS/CSS (Fetch API)，后端采用 Express + SQLite。

## 1. 环境准备
确保服务器已安装 **Node.js** (建议 v16+)。

## 2. 快速启动
1. 进入目录：
   ```bash
   cd node_server
   ```
2. 安装依赖：
   ```bash
   npm install
   ```
3. 启动项目：
   ```bash
   npm start
   ```

## 3. 使用 Docker 启动 (无需安装 Node.js)
如果你想用 Docker 运行这个版本：

1. **构建镜像**：
   ```bash
   docker build -t smart-guardian-node .
   ```
2. **运行容器**：
   ```bash
   docker run -d --name smart-guardian-node -p 5000:5000 -v "$(pwd)/guardian.db:/app/guardian.db" smart-guardian-node
   ```

## 4. API 兼容性
该版本保留了与原 Python 版完全一致的 API 接口，因此你的 ESP32 设备无需修改任何代码即可无缝切换到此 Node.js 服务器。
*   状态上报: `POST /api/v1/status`
*   指令获取: `GET /api/v1/command`
