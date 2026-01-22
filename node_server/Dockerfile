FROM node:18-slim

# 替换为阿里云镜像源以加速 apt-get 下载
RUN sed -i 's/deb.debian.org/mirrors.aliyun.com/g' /etc/apt/sources.list.d/debian.sources

# 安装构建 sqlite3 所需的依赖：python, make, g++
RUN apt-get update && apt-get install -y \
    python3 \
    make \
    g++ \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY package*.json ./
# 使用阿里云 npm 镜像加速安装
RUN npm config set registry https://registry.npmmirror.com && npm install

COPY . .

EXPOSE 5000

CMD ["npm", "start"]
