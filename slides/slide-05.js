// slide-05.js - 03 系统架构设计
const pptxgen = require("pptxgenjs");

const slideConfig = { type: 'content', index: 5, title: '03 系统架构设计' };

function createSlide(pres, theme) {
  const slide = pres.addSlide();
  slide.background = { color: theme.bg };

  slide.addText("03  /  系统架构", {
    x: 0.6, y: 0.35, w: 5, h: 0.3,
    fontSize: 11, fontFace: "Arial",
    color: theme.accent, bold: true, charSpacing: 3
  });

  slide.addText("系统架构设计", {
    x: 0.6, y: 0.65, w: 9, h: 0.5,
    fontSize: 24, fontFace: "Microsoft YaHei",
    color: theme.primary, bold: true
  });

  // 左侧：架构图
  slide.addImage({
    path: "./imgs/整体架构.png",
    x: 0.5, y: 1.3, w: 5.5, h: 3.7
  });

  // 右侧：4层架构说明
  slide.addText("四层架构分离设计", {
    x: 6.3, y: 1.3, w: 3.3, h: 0.35,
    fontSize: 14, fontFace: "Microsoft YaHei",
    color: theme.primary, bold: true
  });

  const layers = [
    { num: "L4", name: "Web 前端", tech: "Vue 3 + TS + Element Plus", desc: "10个管理页面 · ECharts可视化", color: "10B981" },
    { num: "L3", name: "Go 后端", tech: "REST API + SQLite + TCP CLI", desc: "17 API端点 · 25+命令 · 持久化", color: "8B5CF6" },
    { num: "L2", name: "C 控制面", tech: "DPDK SECONDARY + rte_cmdline", desc: "TCP 8086 · 共享内存读写", color: theme.accent2 },
    { num: "L1", name: "C 数据面", tech: "DPDK PRIMARY + PMD 驱动", desc: "用户态轮询 · 批量处理 · 零拷贝", color: theme.accent }
  ];

  layers.forEach((l, i) => {
    const y = 1.75 + i * 0.78;
    // 编号
    slide.addShape(pres.shapes.RECTANGLE, {
      x: 6.3, y: y, w: 0.5, h: 0.65,
      fill: { color: l.color }
    });
    slide.addText(l.num, {
      x: 6.3, y: y, w: 0.5, h: 0.65,
      fontSize: 14, fontFace: "Arial",
      color: "FFFFFF", bold: true,
      align: "center", valign: "middle"
    });
    // 名称
    slide.addText(l.name, {
      x: 6.95, y: y, w: 2.7, h: 0.25,
      fontSize: 12, fontFace: "Microsoft YaHei",
      color: theme.primary, bold: true
    });
    slide.addText(l.tech, {
      x: 6.95, y: y + 0.25, w: 2.7, h: 0.2,
      fontSize: 9, fontFace: "Microsoft YaHei",
      color: l.color
    });
    slide.addText(l.desc, {
      x: 6.95, y: y + 0.45, w: 2.7, h: 0.2,
      fontSize: 8, fontFace: "Microsoft YaHei",
      color: theme.secondary
    });
  });

  // 底部：通信机制
  slide.addShape(pres.shapes.ROUNDED_RECTANGLE, {
    x: 0.6, y: 5.0, w: 8.8, h: 0.4,
    fill: { color: "FFFFFF" }, line: { color: theme.accent, width: 1 },
    rectRadius: 0.05
  });
  slide.addText([
    { text: "IPC通信：", options: { color: theme.accent, bold: true } },
    { text: "共享内存 (rte_memzone)  +  无锁队列 (rte_ring)  +  版本号机制 (rte_atomic64_t)", options: { color: theme.primary } }
  ], {
    x: 0.6, y: 5.0, w: 8.8, h: 0.4,
    fontSize: 11, fontFace: "Microsoft YaHei",
    align: "center", valign: "middle"
  });

  // 页码
  slide.addText("05", {
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
  pres.writeFile({ fileName: "slide-05-preview.pptx" });
}

module.exports = { createSlide, slideConfig };
