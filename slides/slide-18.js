// slide-18.js - 致谢
const pptxgen = require("pptxgenjs");

const slideConfig = { type: 'cover', index: 18, title: '致谢' };

function createSlide(pres, theme) {
  const slide = pres.addSlide();
  slide.background = { color: theme.primary };

  // 装饰圆
  slide.addShape(pres.shapes.OVAL, {
    x: 8.5, y: -1, w: 4, h: 4,
    fill: { color: theme.accent }, line: { type: "none" }
  });
  slide.addShape(pres.shapes.OVAL, {
    x: -1.5, y: 3.5, w: 3, h: 3,
    fill: { color: theme.accent2 }, line: { type: "none" }
  });

  // 章节号
  slide.addText("0  6", {
    x: 0.6, y: 0.5, w: 2, h: 0.4,
    fontSize: 14, fontFace: "Arial",
    color: theme.accent, bold: true, charSpacing: 4
  });

  // 大字"致谢"
  slide.addText("致  谢", {
    x: 0.6, y: 1.5, w: 8, h: 1.5,
    fontSize: 80, fontFace: "Microsoft YaHei",
    color: "FFFFFF", bold: true,
    charSpacing: 12
  });

  slide.addText("ACKNOWLEDGEMENT", {
    x: 0.6, y: 2.95, w: 8, h: 0.4,
    fontSize: 14, fontFace: "Arial",
    color: theme.accent, charSpacing: 8
  });

  // 分隔线
  slide.addShape(pres.shapes.LINE, {
    x: 0.6, y: 3.5, w: 1.2, h: 0,
    line: { color: theme.accent2, width: 3 }
  });

  // 主要内容
  slide.addText("感谢指导老师 徐晶 教授", {
    x: 0.6, y: 3.75, w: 8, h: 0.4,
    fontSize: 18, fontFace: "Microsoft YaHei",
    color: "FFFFFF", bold: true
  });

  slide.addText("在毕业设计推进期间的悉心指导、耐心帮助与专业建议", {
    x: 0.6, y: 4.2, w: 8, h: 0.3,
    fontSize: 12, fontFace: "Microsoft YaHei",
    color: theme.light
  });

  slide.addText("感谢实验室同学 · 感谢贵州大学计算机科学与技术学院", {
    x: 0.6, y: 4.6, w: 8, h: 0.3,
    fontSize: 12, fontFace: "Microsoft YaHei",
    color: theme.light
  });

  // 底部
  slide.addText("感谢各位老师的聆听与指导", {
    x: 0.6, y: 5.05, w: 8, h: 0.35,
    fontSize: 16, fontFace: "Microsoft YaHei",
    color: theme.accent2, bold: true, align: "center"
  });

  // 页码
  slide.addText("18", {
    x: 9.3, y: 5.1, w: 0.4, h: 0.3,
    fontSize: 11, fontFace: "Arial",
    color: theme.accent, align: "right"
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
  pres.writeFile({ fileName: "slide-18-preview.pptx" });
}

module.exports = { createSlide, slideConfig };
