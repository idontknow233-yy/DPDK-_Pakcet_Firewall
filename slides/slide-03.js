// slide-03.js - 01 背景与意义
const pptxgen = require("pptxgenjs");

const slideConfig = { type: 'content', index: 3, title: '01 背景与意义' };

function createSlide(pres, theme) {
  const slide = pres.addSlide();
  slide.background = { color: theme.bg };

  // 顶部章节标签
  slide.addText("01  /  背景与意义", {
    x: 0.6, y: 0.35, w: 5, h: 0.3,
    fontSize: 11, fontFace: "Arial",
    color: theme.accent, bold: true, charSpacing: 3
  });

  // 主标题
  slide.addText("传统防火墙性能瓶颈与DPDK技术优势", {
    x: 0.6, y: 0.65, w: 9, h: 0.5,
    fontSize: 24, fontFace: "Microsoft YaHei",
    color: theme.primary, bold: true
  });

  // 左侧：传统架构瓶颈
  slide.addShape(pres.shapes.RECTANGLE, {
    x: 0.6, y: 1.45, w: 4.4, h: 3.55,
    fill: { color: "FFFFFF" }, line: { color: theme.light, width: 1 }
  });
  slide.addShape(pres.shapes.RECTANGLE, {
    x: 0.6, y: 1.45, w: 0.08, h: 3.55,
    fill: { color: "DC2626" }
  });

  slide.addText("传统内核防火墙", {
    x: 0.85, y: 1.6, w: 3.5, h: 0.35,
    fontSize: 16, fontFace: "Microsoft YaHei",
    color: theme.primary, bold: true
  });
  slide.addText("性能瓶颈", {
    x: 0.85, y: 1.95, w: 3.5, h: 0.3,
    fontSize: 11, fontFace: "Microsoft YaHei",
    color: "DC2626"
  });

  const bottlenecks = [
    { t: "中断开销", d: "频繁硬件中断消耗大量CPU资源" },
    { t: "内存拷贝", d: "DMA→内核→用户态多次拷贝" },
    { t: "上下文切换", d: "用户/内核态切换 + 缓存刷新" },
    { t: "锁竞争", d: "全局数据结构多核争用" }
  ];

  bottlenecks.forEach((b, i) => {
    const y = 2.35 + i * 0.6;
    slide.addShape(pres.shapes.OVAL, {
      x: 0.95, y: y + 0.05, w: 0.3, h: 0.3,
      fill: { color: "FEE2E2" }
    });
    slide.addText((i + 1).toString(), {
      x: 0.95, y: y + 0.05, w: 0.3, h: 0.3,
      fontSize: 12, fontFace: "Arial",
      color: "DC2626", bold: true,
      align: "center", valign: "middle"
    });
    slide.addText(b.t, {
      x: 1.4, y: y, w: 3.5, h: 0.22,
      fontSize: 12, fontFace: "Microsoft YaHei",
      color: theme.primary, bold: true
    });
    slide.addText(b.d, {
      x: 1.4, y: y + 0.22, w: 3.5, h: 0.3,
      fontSize: 10, fontFace: "Microsoft YaHei",
      color: theme.secondary
    });
  });

  // 关键数据：30%~50%
  slide.addShape(pres.shapes.ROUNDED_RECTANGLE, {
    x: 0.85, y: 4.7, w: 3.9, h: 0.22,
    fill: { color: "FEE2E2" }, rectRadius: 0.03
  });
  slide.addText("有效吞吐仅 30%~50%", {
    x: 0.85, y: 4.7, w: 3.9, h: 0.22,
    fontSize: 10, fontFace: "Microsoft YaHei",
    color: "DC2626", bold: true, align: "center", valign: "middle"
  });

  // 右侧：DPDK优势
  slide.addShape(pres.shapes.RECTANGLE, {
    x: 5.2, y: 1.45, w: 4.2, h: 3.55,
    fill: { color: "FFFFFF" }, line: { color: theme.light, width: 1 }
  });
  slide.addShape(pres.shapes.RECTANGLE, {
    x: 5.2, y: 1.45, w: 0.08, h: 3.55,
    fill: { color: theme.accent }
  });

  slide.addText("DPDK 技术优势", {
    x: 5.45, y: 1.6, w: 3.5, h: 0.35,
    fontSize: 16, fontFace: "Microsoft YaHei",
    color: theme.primary, bold: true
  });
  slide.addText("六项关键优化技术", {
    x: 5.45, y: 1.95, w: 3.5, h: 0.3,
    fontSize: 11, fontFace: "Microsoft YaHei",
    color: theme.accent
  });

  const advs = [
    { t: "用户态轮询", d: "绕过中断，零系统调用" },
    { t: "大页内存", d: "2MB/1GB大页，降低TLB Miss" },
    { t: "CPU亲和性", d: "线程绑核，避免缓存污染" },
    { t: "批量处理", d: "32包/批，摊薄函数调用" },
    { t: "零拷贝", d: "网卡通写应用内存池" },
    { t: "多队列RSS", d: "按流分散到多核并行" }
  ];

  advs.forEach((a, i) => {
    const col = i % 2;
    const row = Math.floor(i / 2);
    const x = 5.45 + col * 2.0;
    const y = 2.4 + row * 0.5;
    slide.addShape(pres.shapes.OVAL, {
      x: x, y: y + 0.05, w: 0.18, h: 0.18,
      fill: { color: theme.accent }
    });
    slide.addText(a.t, {
      x: x + 0.25, y: y, w: 1.75, h: 0.22,
      fontSize: 11, fontFace: "Microsoft YaHei",
      color: theme.primary, bold: true
    });
    slide.addText(a.d, {
      x: x + 0.25, y: y + 0.22, w: 1.75, h: 0.24,
      fontSize: 9, fontFace: "Microsoft YaHei",
      color: theme.secondary
    });
  });

  // 底部：研究目标
  slide.addShape(pres.shapes.ROUNDED_RECTANGLE, {
    x: 0.6, y: 5.05, w: 8.8, h: 0.35,
    fill: { color: theme.primary }, rectRadius: 0.05
  });
  slide.addText([
    { text: "系统目标：", options: { color: theme.accent2, bold: true } },
    { text: "高性能 + 可扩展 + 可管理  |  基于DPDK实现线速数据包处理 + 模块化设计 + Web/CLI双管理接口", options: { color: "FFFFFF" } }
  ], {
    x: 0.6, y: 5.05, w: 8.8, h: 0.35,
    fontSize: 11, fontFace: "Microsoft YaHei",
    align: "center", valign: "middle"
  });

  // 页码
  slide.addText("03", {
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
  pres.writeFile({ fileName: "slide-03-preview.pptx" });
}

module.exports = { createSlide, slideConfig };
