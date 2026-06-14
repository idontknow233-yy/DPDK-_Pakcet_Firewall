// slide-04.js - 02 项目规模与工作量 (新增)
const pptxgen = require("pptxgenjs");

const slideConfig = { type: 'content', index: 4, title: '02 项目规模与工作量' };

function createSlide(pres, theme) {
  const slide = pres.addSlide();
  slide.background = { color: theme.bg };

  slide.addText("02  /  项目规模", {
    x: 0.6, y: 0.35, w: 5, h: 0.3,
    fontSize: 11, fontFace: "Arial",
    color: theme.accent, bold: true, charSpacing: 3
  });

  slide.addText("项目规模与工作量", {
    x: 0.6, y: 0.65, w: 9, h: 0.5,
    fontSize: 24, fontFace: "Microsoft YaHei",
    color: theme.primary, bold: true
  });

  // 总览
  slide.addText([
    { text: "14,500", options: { fontSize: 56, bold: true, color: theme.accent } },
    { text: " 行代码  ", options: { fontSize: 14, color: theme.secondary } },
    { text: "·  ", options: { fontSize: 14, color: theme.light } },
    { text: "83", options: { fontSize: 56, bold: true, color: theme.accent2 } },
    { text: " 源文件  ", options: { fontSize: 14, color: theme.secondary } },
    { text: "·  ", options: { fontSize: 14, color: theme.light } },
    { text: "4", options: { fontSize: 56, bold: true, color: theme.primary } },
    { text: " 子项目", options: { fontSize: 14, color: theme.secondary } }
  ], {
    x: 0.6, y: 1.3, w: 8.8, h: 1.0,
    fontFace: "Microsoft YaHei", align: "center", valign: "middle"
  });

  // 4 个子项目卡片
  const subs = [
    { name: "数据面 (DPDK)", lang: "C/C++", lines: "9,589", icon: "DPDK", desc: "ACL / 会话 / 路由 / ARP / IPv6", color: theme.accent },
    { name: "控制面 (CLI)", lang: "C/C++", lines: "2,451", icon: "CLI", desc: "rte_cmdline + TCP 8086服务", color: theme.accent2 },
    { name: "Web 后端 (Go)", lang: "Go", lines: "2,566", icon: "Go", desc: "17 REST API + SQLite持久化", color: "8B5CF6" },
    { name: "Web 前端 (Vue)", lang: "Vue/TS", lines: "2,350", icon: "Vue", desc: "10 页面 + ECharts可视化", color: "10B981" }
  ];

  const cardW = 2.05;
  const cardH = 1.85;
  const startX = 0.6;
  const gap = 0.2;

  subs.forEach((s, i) => {
    const x = startX + i * (cardW + gap);
    const y = 2.55;

    // 卡片背景
    slide.addShape(pres.shapes.ROUNDED_RECTANGLE, {
      x: x, y: y, w: cardW, h: cardH,
      fill: { color: "FFFFFF" }, line: { color: theme.light, width: 1 },
      rectRadius: 0.06
    });
    // 顶部色条
    slide.addShape(pres.shapes.RECTANGLE, {
      x: x, y: y, w: cardW, h: 0.12,
      fill: { color: s.color }
    });

    // 项目名称
    slide.addText(s.name, {
      x: x + 0.15, y: y + 0.2, w: cardW - 0.3, h: 0.3,
      fontSize: 13, fontFace: "Microsoft YaHei",
      color: theme.primary, bold: true
    });

    // 语言标签
    slide.addShape(pres.shapes.ROUNDED_RECTANGLE, {
      x: x + 0.15, y: y + 0.55, w: 0.7, h: 0.22,
      fill: { color: "F1F5F9" }, rectRadius: 0.03
    });
    slide.addText(s.lang, {
      x: x + 0.15, y: y + 0.55, w: 0.7, h: 0.22,
      fontSize: 9, fontFace: "Arial",
      color: theme.secondary, align: "center", valign: "middle"
    });

    // 行数
    slide.addText(s.lines, {
      x: x + 0.15, y: y + 0.85, w: cardW - 0.3, h: 0.55,
      fontSize: 30, fontFace: "Arial",
      color: s.color, bold: true
    });
    slide.addText("行", {
      x: x + 0.15, y: y + 1.35, w: cardW - 0.3, h: 0.2,
      fontSize: 9, fontFace: "Microsoft YaHei",
      color: theme.secondary
    });

    // 描述
    slide.addText(s.desc, {
      x: x + 0.15, y: y + 1.55, w: cardW - 0.3, h: 0.25,
      fontSize: 9, fontFace: "Microsoft YaHei",
      color: theme.secondary
    });
  });

  // 底部：核心产出指标
  slide.addShape(pres.shapes.ROUNDED_RECTANGLE, {
    x: 0.6, y: 4.65, w: 8.8, h: 0.75,
    fill: { color: theme.primary }, rectRadius: 0.06
  });

  const metrics = [
    { v: "17", l: "REST API 端点" },
    { v: "25+", l: "CLI 命令" },
    { v: "10", l: "前端功能页面" },
    { v: "40万", l: "并发会话容量" },
    { v: "8", l: "处理流水线阶段" }
  ];

  metrics.forEach((m, i) => {
    const x = 0.7 + i * 1.74;
    slide.addText(m.v, {
      x: x, y: 4.7, w: 1.7, h: 0.4,
      fontSize: 22, fontFace: "Arial",
      color: theme.accent2, bold: true, align: "center"
    });
    slide.addText(m.l, {
      x: x, y: 5.08, w: 1.7, h: 0.3,
      fontSize: 9, fontFace: "Microsoft YaHei",
      color: "FFFFFF", align: "center"
    });
  });

  // 页码
  slide.addText("04", {
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
  pres.writeFile({ fileName: "slide-04-preview.pptx" });
}

module.exports = { createSlide, slideConfig };
