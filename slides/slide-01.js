// slide-01.js - 封面
const pptxgen = require("pptxgenjs");

const slideConfig = { type: 'cover', index: 1, title: '封面' };

function createSlide(pres, theme) {
  const slide = pres.addSlide();
  slide.background = { color: theme.bg };

  // 顶部装饰条
  slide.addShape(pres.shapes.RECTANGLE, {
    x: 0, y: 0, w: 10, h: 0.08,
    fill: { color: theme.accent }
  });

  // 底部装饰条
  slide.addShape(pres.shapes.RECTANGLE, {
    x: 0, y: 5.545, w: 10, h: 0.08,
    fill: { color: theme.accent }
  });

  // 左侧强调竖条
  slide.addShape(pres.shapes.RECTANGLE, {
    x: 0.8, y: 1.5, w: 0.12, h: 2.2,
    fill: { color: theme.accent2 }
  });

  // 答辩标识
  slide.addText("本科毕业答辩", {
    x: 1.2, y: 1.5, w: 3, h: 0.4,
    fontSize: 16, fontFace: "Microsoft YaHei",
    color: theme.secondary, margin: 0
  });

  // 主标题
  slide.addText("基于DPDK的高性能\n数据包防火墙设计与实现", {
    x: 1.2, y: 1.95, w: 7.5, h: 1.8,
    fontSize: 38, fontFace: "Microsoft YaHei",
    color: theme.primary, bold: true, lineSpacingMultiple: 1.25
  });

  // 英文副标题
  slide.addText("High-Performance Packet Firewall Based on DPDK", {
    x: 1.2, y: 3.75, w: 7, h: 0.4,
    fontSize: 13, fontFace: "Arial",
    color: theme.secondary, italic: true
  });

  // 关键标签：工作量标记
  slide.addShape(pres.shapes.ROUNDED_RECTANGLE, {
    x: 1.2, y: 4.25, w: 2.7, h: 0.32,
    fill: { color: theme.accent }, rectRadius: 0.05
  });
  slide.addText("14,500+ 行代码 · 4 子项目", {
    x: 1.2, y: 4.25, w: 2.7, h: 0.32,
    fontSize: 11, fontFace: "Microsoft YaHei",
    color: "FFFFFF", bold: true, align: "center", valign: "middle"
  });

  slide.addShape(pres.shapes.ROUNDED_RECTANGLE, {
    x: 4.0, y: 4.25, w: 2.4, h: 0.32,
    fill: { color: theme.accent2 }, rectRadius: 0.05
  });
  slide.addText("DPDK 24.11 · IPv4/IPv6 双栈", {
    x: 4.0, y: 4.25, w: 2.4, h: 0.32,
    fontSize: 11, fontFace: "Microsoft YaHei",
    color: "FFFFFF", bold: true, align: "center", valign: "middle"
  });

  // 作者信息
  slide.addText([
    { text: "答辩人：杨毅", options: { breakLine: true } },
    { text: "指导教师：徐晶  教授", options: { breakLine: true } },
    { text: "计算机科学与技术学院", options: {} }
  ], {
    x: 1.2, y: 4.75, w: 5, h: 0.85,
    fontSize: 13, fontFace: "Microsoft YaHei",
    color: theme.secondary, lineSpacingMultiple: 1.5
  });

  slide.addText("2025年5月", {
    x: 7.5, y: 4.95, w: 2, h: 0.3,
    fontSize: 12, fontFace: "Arial",
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
  pres.writeFile({ fileName: "slide-01-preview.pptx" });
}

module.exports = { createSlide, slideConfig };
