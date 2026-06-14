// slide-13.js - 11 测试环境
const pptxgen = require("pptxgenjs");

const slideConfig = { type: 'content', index: 13, title: '11 测试环境' };

function createSlide(pres, theme) {
  const slide = pres.addSlide();
  slide.background = { color: theme.bg };

  slide.addText("11  /  测试环境", {
    x: 0.6, y: 0.35, w: 5, h: 0.3,
    fontSize: 11, fontFace: "Arial",
    color: theme.accent, bold: true, charSpacing: 3
  });

  slide.addText("测试环境", {
    x: 0.6, y: 0.65, w: 9, h: 0.5,
    fontSize: 24, fontFace: "Microsoft YaHei",
    color: theme.primary, bold: true
  });

  // 左侧：功能测试环境
  slide.addShape(pres.shapes.RECTANGLE, {
    x: 0.6, y: 1.3, w: 4.4, h: 3.6,
    fill: { color: "FFFFFF" }, line: { color: theme.light, width: 1 }
  });
  slide.addShape(pres.shapes.RECTANGLE, {
    x: 0.6, y: 1.3, w: 4.4, h: 0.45,
    fill: { color: theme.accent }
  });
  slide.addText("功能测试环境", {
    x: 0.75, y: 1.3, w: 4.2, h: 0.45,
    fontSize: 14, fontFace: "Microsoft YaHei",
    color: "FFFFFF", bold: true, valign: "middle"
  });
  slide.addText("目标部署硬件 (Xeon E3 · RTL8111)", {
    x: 0.75, y: 1.4, w: 4.2, h: 0.3,
    fontSize: 9, fontFace: "Microsoft YaHei",
    color: theme.light, valign: "middle", align: "right"
  });

  // 硬件配置
  const hwConfig = [
    { k: "CPU", v: "Intel Xeon E3-1230 v3 (4核8线程, 3.30GHz)" },
    { k: "网卡", v: "Realtek RTL8111/8168 双口千兆" },
    { k: "内存", v: "Kingston DDR3 8GB" },
    { k: "存储", v: "金泰克 S300 120GB SSD" }
  ];
  hwConfig.forEach((c, i) => {
    const y = 1.95 + i * 0.32;
    slide.addShape(pres.shapes.RECTANGLE, {
      x: 0.8, y: y + 0.05, w: 0.7, h: 0.22,
      fill: { color: theme.accent }
    });
    slide.addText(c.k, {
      x: 0.8, y: y + 0.05, w: 0.7, h: 0.22,
      fontSize: 9, fontFace: "Microsoft YaHei",
      color: "FFFFFF", bold: true, align: "center", valign: "middle"
    });
    slide.addText(c.v, {
      x: 1.6, y: y + 0.05, w: 3.3, h: 0.22,
      fontSize: 9, fontFace: "Microsoft YaHei",
      color: theme.primary, valign: "middle"
    });
  });

  // 软件环境
  slide.addText("软件环境", {
    x: 0.8, y: 3.3, w: 2, h: 0.25,
    fontSize: 10, fontFace: "Microsoft YaHei",
    color: theme.primary, bold: true
  });
  slide.addText("Ubuntu 22.04 LTS · 内核 6.8 · DPDK 24.11 · GCC 11.4", {
    x: 0.8, y: 3.55, w: 4.1, h: 0.25,
    fontSize: 9, fontFace: "Microsoft YaHei",
    color: theme.secondary
  });

  // PMD 说明
  slide.addShape(pres.shapes.ROUNDED_RECTANGLE, {
    x: 0.8, y: 3.9, w: 4.0, h: 0.9,
    fill: { color: "F0FDF9" }, line: { color: theme.accent, width: 0.5 },
    rectRadius: 0.04
  });
  slide.addText("AF_PACKET PMD", {
    x: 0.95, y: 3.95, w: 3.5, h: 0.25,
    fontSize: 10, fontFace: "Microsoft YaHei",
    color: theme.accent, bold: true
  });
  slide.addText("Realtek 无原生PMD，使用AF_PACKET软件轮询驱动；功能API与原生PMD完全兼容，不影响逻辑正确性验证", {
    x: 0.95, y: 4.2, w: 3.8, h: 0.6,
    fontSize: 8, fontFace: "Microsoft YaHei",
    color: theme.secondary, lineSpacingMultiple: 1.3
  });

  // 右侧：性能测试环境
  slide.addShape(pres.shapes.RECTANGLE, {
    x: 5.2, y: 1.3, w: 4.2, h: 3.6,
    fill: { color: "FFFFFF" }, line: { color: theme.light, width: 1 }
  });
  slide.addShape(pres.shapes.RECTANGLE, {
    x: 5.2, y: 1.3, w: 4.2, h: 0.45,
    fill: { color: theme.accent2 }
  });
  slide.addText("性能测试环境", {
    x: 5.35, y: 1.3, w: 4.0, h: 0.45,
    fontSize: 14, fontFace: "Microsoft YaHei",
    color: "FFFFFF", bold: true, valign: "middle"
  });
  slide.addText("VMware + VMXNET3 (DPDK原生支持)", {
    x: 5.35, y: 1.4, w: 4.0, h: 0.3,
    fontSize: 9, fontFace: "Microsoft YaHei",
    color: theme.light, valign: "middle", align: "right"
  });

  // 防火墙 VM
  slide.addShape(pres.shapes.ROUNDED_RECTANGLE, {
    x: 5.4, y: 1.95, w: 1.9, h: 1.4,
    fill: { color: "FFF7ED" }, line: { color: theme.accent2, width: 0.5 },
    rectRadius: 0.04
  });
  slide.addText("防火墙 VM", {
    x: 5.5, y: 2.0, w: 1.7, h: 0.25,
    fontSize: 10, fontFace: "Microsoft YaHei",
    color: theme.accent2, bold: true
  });
  slide.addText([
    { text: "6 vCPU / 6GB", options: { breakLine: true } },
    { text: "2 × VMXNET3 网卡", options: { breakLine: true } },
    { text: "2048 × 2MB 大页", options: { breakLine: true } },
    { text: "Ubuntu 22.04 LTS", options: {} }
  ], {
    x: 5.5, y: 2.3, w: 1.7, h: 1.0,
    fontSize: 8, fontFace: "Microsoft YaHei",
    color: theme.primary, lineSpacingMultiple: 1.4
  });

  // 发包器 VM
  slide.addShape(pres.shapes.ROUNDED_RECTANGLE, {
    x: 7.4, y: 1.95, w: 1.9, h: 1.4,
    fill: { color: "F0FDF9" }, line: { color: theme.accent, width: 0.5 },
    rectRadius: 0.04
  });
  slide.addText("DPDK 发包器 VM", {
    x: 7.5, y: 2.0, w: 1.7, h: 0.25,
    fontSize: 10, fontFace: "Microsoft YaHei",
    color: theme.accent, bold: true
  });
  slide.addText([
    { text: "2 vCPU / 2GB", options: { breakLine: true } },
    { text: "1 × VMXNET3 网卡", options: { breakLine: true } },
    { text: "512 × 2MB 大页", options: { breakLine: true } },
    { text: "全速发包模式", options: {} }
  ], {
    x: 7.5, y: 2.3, w: 1.7, h: 1.0,
    fontSize: 8, fontFace: "Microsoft YaHei",
    color: theme.primary, lineSpacingMultiple: 1.4
  });

  // 拓扑说明
  slide.addText("测试拓扑", {
    x: 5.4, y: 3.5, w: 2, h: 0.25,
    fontSize: 10, fontFace: "Microsoft YaHei",
    color: theme.primary, bold: true
  });
  slide.addText("VMnet2 传输测试流量  ·  VMnet3 接收转发后流量 (避免回流)", {
    x: 5.4, y: 3.75, w: 3.9, h: 0.25,
    fontSize: 8, fontFace: "Microsoft YaHei",
    color: theme.secondary
  });

  // 包大小
  slide.addShape(pres.shapes.ROUNDED_RECTANGLE, {
    x: 5.4, y: 4.05, w: 3.9, h: 0.75,
    fill: { color: theme.primary }, rectRadius: 0.04
  });
  slide.addText("测试包大小：6 档", {
    x: 5.5, y: 4.1, w: 3.7, h: 0.25,
    fontSize: 10, fontFace: "Microsoft YaHei",
    color: theme.accent2, bold: true
  });
  slide.addText("64B · 128B · 256B · 512B · 1024B · 1514B  (各 30s)", {
    x: 5.5, y: 4.35, w: 3.7, h: 0.4,
    fontSize: 10, fontFace: "Arial",
    color: "FFFFFF"
  });

  slide.addText("13", {
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
  pres.writeFile({ fileName: "slide-13-preview.pptx" });
}

module.exports = { createSlide, slideConfig };
