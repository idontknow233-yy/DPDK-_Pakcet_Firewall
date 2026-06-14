// slide-02.js - 目录
const pptxgen = require("pptxgenjs");

const slideConfig = { type: 'toc', index: 2, title: '目录' };

function createSlide(pres, theme) {
  const slide = pres.addSlide();
  slide.background = { color: theme.bg };

  // 顶部小色条
  slide.addShape(pres.shapes.RECTANGLE, {
    x: 0.8, y: 0.45, w: 0.4, h: 0.06,
    fill: { color: theme.accent2 }
  });

  // 主标题
  slide.addText("目  录", {
    x: 0.8, y: 0.55, w: 5, h: 0.6,
    fontSize: 32, fontFace: "Microsoft YaHei",
    color: theme.primary, bold: true
  });

  slide.addText("CONTENTS", {
    x: 0.8, y: 1.1, w: 5, h: 0.3,
    fontSize: 12, fontFace: "Arial",
    color: theme.secondary, charSpacing: 4
  });

  // 右侧装饰：大数字
  slide.addText("5", {
    x: 7.5, y: 0.4, w: 2, h: 1.0,
    fontSize: 80, fontFace: "Arial",
    color: theme.light, bold: true, align: "right"
  });
  slide.addText("PART", {
    x: 7.5, y: 1.3, w: 2, h: 0.3,
    fontSize: 12, fontFace: "Arial",
    color: theme.secondary, align: "right", charSpacing: 3
  });

  // 5个章节
  const sections = [
    { num: "01", title: "背景与意义", sub: "DPDK技术与系统目标" },
    { num: "02", title: "项目规模与系统架构", sub: "代码量级 + 四层架构 + 流水线" },
    { num: "03", title: "核心模块实现", sub: "ACL · 会话跟踪 · 路由 · IPC · CLI/Web" },
    { num: "04", title: "性能优化与测试", sub: "7大优化 + 功能测试 + 性能测试" },
    { num: "05", title: "总结与展望", sub: "系统亮点 + 不足与改进方向" }
  ];

  const startY = 1.75;
  const rowH = 0.68;

  sections.forEach((s, i) => {
    const y = startY + i * rowH;

    // 序号方块
    slide.addShape(pres.shapes.RECTANGLE, {
      x: 0.8, y: y, w: 0.7, h: 0.55,
      fill: { color: theme.accent }
    });
    slide.addText(s.num, {
      x: 0.8, y: y, w: 0.7, h: 0.55,
      fontSize: 20, fontFace: "Arial",
      color: "FFFFFF", bold: true,
      align: "center", valign: "middle"
    });

    // 主标题
    slide.addText(s.title, {
      x: 1.7, y: y, w: 5, h: 0.32,
      fontSize: 18, fontFace: "Microsoft YaHei",
      color: theme.primary, bold: true, valign: "middle"
    });

    // 副标题
    slide.addText(s.sub, {
      x: 1.7, y: y + 0.3, w: 6, h: 0.26,
      fontSize: 11, fontFace: "Microsoft YaHei",
      color: theme.secondary
    });

    // 右侧连接线
    if (i < sections.length - 1) {
      slide.addShape(pres.shapes.LINE, {
        x: 1.15, y: y + 0.55, w: 0, h: rowH - 0.55,
        line: { color: theme.accent, width: 1, dashType: "dash" }
      });
    }
  });

  // 页码徽章
  slide.addShape(pres.shapes.OVAL, {
    x: 9.3, y: 5.1, w: 0.4, h: 0.4,
    fill: { color: theme.accent }
  });
  slide.addText("02", {
    x: 9.3, y: 5.1, w: 0.4, h: 0.4,
    fontSize: 11, fontFace: "Arial",
    color: "FFFFFF", bold: true,
    align: "center", valign: "middle"
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
  pres.writeFile({ fileName: "slide-02-preview.pptx" });
}

module.exports = { createSlide, slideConfig };
