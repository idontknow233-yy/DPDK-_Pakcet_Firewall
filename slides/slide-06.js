// slide-06.js - 04 数据面处理流水线
const pptxgen = require("pptxgenjs");

const slideConfig = { type: 'content', index: 6, title: '04 数据面处理流水线' };

function createSlide(pres, theme) {
  const slide = pres.addSlide();
  slide.background = { color: theme.bg };

  slide.addText("04  /  核心模块", {
    x: 0.6, y: 0.35, w: 5, h: 0.3,
    fontSize: 11, fontFace: "Arial",
    color: theme.accent, bold: true, charSpacing: 3
  });

  slide.addText("数据面处理流水线", {
    x: 0.6, y: 0.65, w: 9, h: 0.5,
    fontSize: 24, fontFace: "Microsoft YaHei",
    color: theme.primary, bold: true
  });

  // 左侧：流水线图
  slide.addImage({
    path: "./imgs/数据包处理流水线.png",
    x: 0.4, y: 1.3, w: 5.0, h: 3.7
  });

  // 右侧：8个阶段详细说明
  slide.addText("8 阶段流水线处理", {
    x: 5.7, y: 1.3, w: 4, h: 0.32,
    fontSize: 14, fontFace: "Microsoft YaHei",
    color: theme.primary, bold: true
  });

  const stages = [
    { n: "1", t: "批量收包", d: "rte_eth_rx_burst · 32包/批" },
    { n: "2", t: "协议解析", d: "IPv4/IPv6/ARP 分派" },
    { n: "3", t: "攻击检测", d: "端口扫描封禁 · SYN/UDP统计" },
    { n: "4", t: "速率限制", d: "令牌桶 · TCP SYN/UDP" },
    { n: "5", t: "ACL检查", d: "五元组线性匹配" },
    { n: "6", t: "连接跟踪", d: "rte_hash O(1) · per-lcore" },
    { n: "7", t: "路由转发", d: "LPM查找 · ARP/ND解析" },
    { n: "8", t: "批量发送", d: "TX缓冲排空 · ~100μs周期" }
  ];

  stages.forEach((s, i) => {
    const col = i % 2;
    const row = Math.floor(i / 2);
    const x = 5.7 + col * 2.0;
    const y = 1.75 + row * 0.42;

    // 序号圆
    slide.addShape(pres.shapes.OVAL, {
      x: x, y: y + 0.03, w: 0.3, h: 0.3,
      fill: { color: theme.accent }
    });
    slide.addText(s.n, {
      x: x, y: y + 0.03, w: 0.3, h: 0.3,
      fontSize: 11, fontFace: "Arial",
      color: "FFFFFF", bold: true,
      align: "center", valign: "middle"
    });
    // 标题
    slide.addText(s.t, {
      x: x + 0.38, y: y, w: 1.6, h: 0.2,
      fontSize: 11, fontFace: "Microsoft YaHei",
      color: theme.primary, bold: true
    });
    // 描述
    slide.addText(s.d, {
      x: x + 0.38, y: y + 0.2, w: 1.6, h: 0.2,
      fontSize: 8, fontFace: "Microsoft YaHei",
      color: theme.secondary
    });
  });

  // 底部：多核模型
  slide.addShape(pres.shapes.ROUNDED_RECTANGLE, {
    x: 0.6, y: 5.0, w: 8.8, h: 0.4,
    fill: { color: theme.primary }, rectRadius: 0.05
  });
  slide.addText([
    { text: "多核处理模型：", options: { color: theme.accent2, bold: true } },
    { text: "收包核 + 处理核 + 发包核  ·  RSS保证流亲和性  ·  每核独立无锁快速路径", options: { color: "FFFFFF" } }
  ], {
    x: 0.6, y: 5.0, w: 8.8, h: 0.4,
    fontSize: 11, fontFace: "Microsoft YaHei",
    align: "center", valign: "middle"
  });

  // 页码
  slide.addText("06", {
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
  pres.writeFile({ fileName: "slide-06-preview.pptx" });
}

module.exports = { createSlide, slideConfig };
