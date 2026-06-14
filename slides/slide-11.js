// slide-11.js - 09 Web 管理平台 (含前端模拟截图)
const pptxgen = require("pptxgenjs");

const slideConfig = { type: 'content', index: 11, title: '09 Web管理平台' };

function createSlide(pres, theme) {
  const slide = pres.addSlide();
  slide.background = { color: theme.bg };

  slide.addText("09  /  管理接口", {
    x: 0.6, y: 0.35, w: 5, h: 0.3,
    fontSize: 11, fontFace: "Arial",
    color: theme.accent, bold: true, charSpacing: 3
  });

  slide.addText("Web 管理平台", {
    x: 0.6, y: 0.65, w: 9, h: 0.5,
    fontSize: 24, fontFace: "Microsoft YaHei",
    color: theme.primary, bold: true
  });

  // 左侧：技术栈 + 指标
  slide.addText("技术栈", {
    x: 0.6, y: 1.3, w: 3, h: 0.3,
    fontSize: 13, fontFace: "Microsoft YaHei",
    color: theme.primary, bold: true
  });

  const stack = [
    { c: "10B981", t: "Vue 3.4", d: "Composition API" },
    { c: "3B82F6", t: "TypeScript", d: "类型安全" },
    { c: "F97316", t: "Element Plus", d: "UI 组件库" },
    { c: "0D9488", t: "ECharts 5.5", d: "数据可视化" },
    { c: "8B5CF6", t: "Pinia", d: "状态管理" },
    { c: "EF4444", t: "Axios", d: "HTTP 客户端" }
  ];

  stack.forEach((s, i) => {
    const col = i % 2;
    const row = Math.floor(i / 2);
    const x = 0.6 + col * 1.55;
    const y = 1.65 + row * 0.45;
    slide.addShape(pres.shapes.ROUNDED_RECTANGLE, {
      x: x, y: y, w: 1.5, h: 0.4,
      fill: { color: s.c }, rectRadius: 0.04
    });
    slide.addText(s.t, {
      x: x, y: y, w: 1.5, h: 0.2,
      fontSize: 10, fontFace: "Microsoft YaHei",
      color: "FFFFFF", bold: true, align: "center"
    });
    slide.addText(s.d, {
      x: x, y: y + 0.2, w: 1.5, h: 0.2,
      fontSize: 8, fontFace: "Microsoft YaHei",
      color: "FFFFFF", align: "center"
    });
  });

  // 关键指标
  slide.addShape(pres.shapes.ROUNDED_RECTANGLE, {
    x: 0.6, y: 3.65, w: 3.05, h: 1.3,
    fill: { color: "FFFFFF" }, line: { color: theme.light, width: 1 },
    rectRadius: 0.05
  });
  slide.addText("平台规模", {
    x: 0.75, y: 3.7, w: 2, h: 0.25,
    fontSize: 11, fontFace: "Microsoft YaHei",
    color: theme.primary, bold: true
  });
  const metrics = [
    { v: "10", l: "功能页面" },
    { v: "17", l: "REST API" },
    { v: "5s", l: "自动刷新" }
  ];
  metrics.forEach((m, i) => {
    const x = 0.75 + i * 0.95;
    slide.addText(m.v, {
      x: x, y: 3.95, w: 0.9, h: 0.45,
      fontSize: 24, fontFace: "Arial",
      color: theme.accent, bold: true, align: "center"
    });
    slide.addText(m.l, {
      x: x, y: 4.4, w: 0.9, h: 0.2,
      fontSize: 9, fontFace: "Microsoft YaHei",
      color: theme.secondary, align: "center"
    });
  });
  slide.addText("10 个页面：仪表盘 · 拓扑 · ACL · 路由 · 端口 · DDoS · 会话 · 攻击演示", {
    x: 0.75, y: 4.65, w: 2.8, h: 0.25,
    fontSize: 8, fontFace: "Microsoft YaHei",
    color: theme.secondary
  });

  // 右侧：Web 前端 UI 模拟 - 仪表盘
  const uiX = 3.95, uiY = 1.3, uiW = 5.45, uiH = 3.65;

  // 浏览器框架
  slide.addShape(pres.shapes.ROUNDED_RECTANGLE, {
    x: uiX, y: uiY, w: uiW, h: uiH,
    fill: { color: "FFFFFF" }, line: { color: theme.light, width: 1 },
    rectRadius: 0.05
  });

  // 顶部浏览器栏
  slide.addShape(pres.shapes.RECTANGLE, {
    x: uiX, y: uiY, w: uiW, h: 0.32,
    fill: { color: "F1F5F9" }, rectRadius: 0.05
  });
  slide.addShape(pres.shapes.OVAL, { x: uiX + 0.1, y: uiY + 0.1, w: 0.12, h: 0.12, fill: { color: "EF4444" } });
  slide.addShape(pres.shapes.OVAL, { x: uiX + 0.28, y: uiY + 0.1, w: 0.12, h: 0.12, fill: { color: "F59E0B" } });
  slide.addShape(pres.shapes.OVAL, { x: uiX + 0.46, y: uiY + 0.1, w: 0.12, h: 0.12, fill: { color: "10B981" } });
  slide.addShape(pres.shapes.RECTANGLE, {
    x: uiX + 0.7, y: uiY + 0.05, w: uiW - 0.8, h: 0.22,
    fill: { color: "FFFFFF" }, line: { color: theme.light, width: 0.5 }
  });
  slide.addText("http://localhost:5173/dashboard", {
    x: uiX + 0.75, y: uiY + 0.05, w: uiW - 0.9, h: 0.22,
    fontSize: 7, fontFace: "Arial",
    color: theme.secondary, valign: "middle"
  });

  // 左侧导航
  slide.addShape(pres.shapes.RECTANGLE, {
    x: uiX, y: uiY + 0.32, w: 1.1, h: uiH - 0.32,
    fill: { color: "1E293B" }
  });
  slide.addText("🛡 防火墙", {
    x: uiX + 0.1, y: uiY + 0.4, w: 1.0, h: 0.25,
    fontSize: 9, fontFace: "Microsoft YaHei",
    color: "FFFFFF", bold: true
  });

  const navItems = ["📊 仪表盘", "🔗 拓扑", "🛡 ACL规则", "🌐 路由表", "⚙ 端口配置", "⚡ DDoS防护", "📋 会话表", "🎯 攻击演示"];
  navItems.forEach((n, i) => {
    const y = uiY + 0.75 + i * 0.3;
    if (i === 0) {
      slide.addShape(pres.shapes.RECTANGLE, {
        x: uiX, y: y, w: 1.1, h: 0.28,
        fill: { color: theme.accent }
      });
    }
    slide.addText(n, {
      x: uiX + 0.05, y: y, w: 1.05, h: 0.28,
      fontSize: 8, fontFace: "Microsoft YaHei",
      color: i === 0 ? "FFFFFF" : "CBD5E1", valign: "middle"
    });
  });

  // 主内容区
  const mainX = uiX + 1.1, mainY = uiY + 0.32, mainW = uiW - 1.1, mainH = uiH - 0.32;

  // 顶部状态条
  slide.addShape(pres.shapes.RECTANGLE, {
    x: mainX, y: mainY, w: mainW, h: 0.32,
    fill: { color: "FFFFFF" }, line: { color: theme.light, width: 0.5 }
  });
  slide.addText("仪表盘", {
    x: mainX + 0.15, y: mainY, w: 1, h: 0.32,
    fontSize: 10, fontFace: "Microsoft YaHei",
    color: theme.primary, bold: true, valign: "middle"
  });
  slide.addShape(pres.shapes.OVAL, {
    x: mainX + mainW - 0.6, y: mainY + 0.1, w: 0.12, h: 0.12,
    fill: { color: "10B981" }
  });
  slide.addText("健康", {
    x: mainX + mainW - 0.45, y: mainY, w: 0.4, h: 0.32,
    fontSize: 8, fontFace: "Microsoft YaHei",
    color: "10B981", valign: "middle"
  });

  // 4个KPI卡片
  const kpis = [
    { v: "1.2M", l: "RX pps", c: theme.accent },
    { v: "1.1M", l: "TX pps", c: "3B82F6" },
    { v: "2", l: "ACL规则", c: theme.accent2 },
    { v: "0", l: "丢包", c: "10B981" }
  ];
  kpis.forEach((k, i) => {
    const x = mainX + 0.1 + i * 1.05;
    const y = mainY + 0.45;
    slide.addShape(pres.shapes.RECTANGLE, {
      x: x, y: y, w: 1.0, h: 0.7,
      fill: { color: "FFFFFF" }, line: { color: theme.light, width: 0.5 }
    });
    slide.addText(k.v, {
      x: x, y: y + 0.1, w: 1.0, h: 0.32,
      fontSize: 16, fontFace: "Arial",
      color: k.c, bold: true, align: "center"
    });
    slide.addText(k.l, {
      x: x, y: y + 0.42, w: 1.0, h: 0.22,
      fontSize: 7, fontFace: "Microsoft YaHei",
      color: theme.secondary, align: "center"
    });
  });

  // 流量趋势图（模拟）
  slide.addShape(pres.shapes.RECTANGLE, {
    x: mainX + 0.1, y: mainY + 1.2, w: 4.1, h: 1.0,
    fill: { color: "FFFFFF" }, line: { color: theme.light, width: 0.5 }
  });
  slide.addText("流量趋势 (pps)", {
    x: mainX + 0.2, y: mainY + 1.22, w: 2, h: 0.2,
    fontSize: 7, fontFace: "Microsoft YaHei",
    color: theme.secondary
  });

  // 绘制折线（模拟）
  const chartX = mainX + 0.3, chartY = mainY + 1.5, chartW = 3.8, chartH = 0.6;
  const points = [0.2, 0.5, 0.4, 0.7, 0.6, 0.85, 0.7, 0.9, 0.75, 0.95, 0.85, 0.8];
  for (let i = 0; i < points.length - 1; i++) {
    const x1 = chartX + (i / (points.length - 1)) * chartW;
    const y1 = chartY + chartH - points[i] * chartH;
    const x2 = chartX + ((i + 1) / (points.length - 1)) * chartW;
    const y2 = chartY + chartH - points[i + 1] * chartH;
    slide.addShape(pres.shapes.LINE, {
      x: x1, y: y1, w: x2 - x1, h: y2 - y1,
      line: { color: theme.accent, width: 1.5 }
    });
  }

  // 最近拒绝日志（模拟）
  slide.addShape(pres.shapes.RECTANGLE, {
    x: mainX + 0.1, y: mainY + 2.3, w: 4.1, h: 0.95,
    fill: { color: "FFFFFF" }, line: { color: theme.light, width: 0.5 }
  });
  slide.addText("最近拦截记录", {
    x: mainX + 0.2, y: mainY + 2.32, w: 2, h: 0.2,
    fontSize: 7, fontFace: "Microsoft YaHei",
    color: theme.secondary
  });

  const logs = [
    "12:34:56  UDP  10.0.0.1 → 8.8.8.8:53  [DENY]",
    "12:34:55  TCP  192.168.1.100 → 80  [DENY]",
    "12:34:50  UDP  10.0.0.5 → 1.1.1.1:53  [DENY]"
  ];
  logs.forEach((l, i) => {
    slide.addText(l, {
      x: mainX + 0.2, y: mainY + 2.55 + i * 0.2, w: 4.0, h: 0.2,
      fontSize: 6, fontFace: "Consolas",
      color: "DC2626"
    });
  });

  // 底部说明
  slide.addShape(pres.shapes.ROUNDED_RECTANGLE, {
    x: 0.6, y: 5.0, w: 8.8, h: 0.4,
    fill: { color: theme.primary }, rectRadius: 0.05
  });
  slide.addText([
    { text: "Vue 3 SPA：", options: { color: theme.accent2, bold: true } },
    { text: "Composition API + 路由 + Pinia + 5s轮询 + ECharts  ·  ", options: { color: "FFFFFF" } },
    { text: "Go 后端", options: { color: theme.accent, bold: true } },
    { text: " 17 REST API + SQLite 持久化 + 启动自动同步", options: { color: "FFFFFF" } }
  ], {
    x: 0.6, y: 5.0, w: 8.8, h: 0.4,
    fontSize: 10, fontFace: "Microsoft YaHei",
    align: "center", valign: "middle"
  });

  slide.addText("11", {
    x: 9.3, y: 5.1, w: 0.4, h: 0.3,
    fontSize: 11, fontFace: "Arial",
    color: theme.secondary, align: "right"
  });

  return slide;
}

if (require.main === module) {
  const pres = new pptxgen();
  pres.layout = 'LAYOUT_16x9';
  const theme = {
    primary: "1E293B", secondary: "64748B", accent: "0D9488",
    accent2: "F97316", light: "F0FDF9", bg: "F5F7FA"
  };
  createSlide(pres, theme);
  pres.writeFile({ fileName: "slide-11-preview.pptx" });
}

module.exports = { createSlide, slideConfig };
