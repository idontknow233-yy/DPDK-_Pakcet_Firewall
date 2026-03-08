开发：
1. 安装 Node.js 18+
2. 进入 web/frontend 执行：npm install
3. 运行开发环境：npm run dev

环境变量：
如果浏览器与开发机不在同一台机器，不要把 VITE_API_BASE 写成 127.0.0.1
推荐做法：保持 VITE_API_BASE 为空，走 Vite 代理，把 /api 代理到后端网关 9000 端口

说明：
Vite 已配置 /api -> http://127.0.0.1:9000 代理
