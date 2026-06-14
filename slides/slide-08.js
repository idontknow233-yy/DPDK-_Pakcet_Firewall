// slide-08.js - 06 ACL 与 会话跟踪
const pptxgen = require("pptxgenjs");

const slideConfig = { type: 'content', index: 8, title: '06 ACL与会话跟踪' };

function createSlide(pres, theme) {
  const slide = pres.addSlide();
  slide.background = { color: theme.bg };

  slide.addText("06  /  核心模块", {
    x: 0.6, y: 0.35, w: 5, h: 0.3,
    fontSize: 11, fontFace: "Arial",
    color: theme.accent, bold: true, charSpacing: 3
  });

  slide.addText("ACL与会话跟踪模块", {
    x: 0.6, y: 0.65, w: 9, h: 0.5,
    fontSize: 24, fontFace: "Microsoft YaHei",
    color: theme.primary, bold: true
  });

  // 左侧：ACL 模块
  slide.addShape(pres.shapes.RECTANGLE, {
    x: 0.6, y: 1.3, w: 4.4, h: 3.65,
    fill: { color: "FFFFFF" }, line: { color: theme.light, width: 1 }
  });
  slide.addShape(pres.shapes.RECTANGLE, {
    x: 0.6, y: 1.3, w: 4.4, h: 0.5,
    fill: { color: theme.accent }
  });
  slide.addText("ACL 五元组规则引擎", {
    x: 0.7, y: 1.3, w: 4.2, h: 0.5,
    fontSize: 15, fontFace: "Microsoft YaHei",
    color: "FFFFFF", bold: true, valign: "middle"
  });

  // 规则字段
  const fields = [
    { k: "源IP", v: "src_ip / src_mask (CIDR)" },
    { k: "目的IP", v: "dst_ip / dst_mask (CIDR)" },
    { k: "源端口", v: "src_port_min ~ max (范围)" },
    { k: "目的端口", v: "dst_port_min ~ max (范围)" },
    { k: "协议", v: "TCP / UDP / ICMP" },
    { k: "动作", v: "allow=1  /  deny=0" }
  ];

  fields.forEach((f, i) => {
    const y = 1.95 + i * 0.32;
    slide.addShape(pres.shapes.RECTANGLE, {
      x: 0.8, y: y + 0.08, w: 0.8, h: 0.2,
      fill: { color: theme.light }
    });
    slide.addText(f.k, {
      x: 0.8, y: y + 0.08, w: 0.8, h: 0.2,
      fontSize: 9, fontFace: "Microsoft YaHei",
      color: theme.accent, bold: true, align: "center", valign: "middle"
    });
    slide.addText(f.v, {
      x: 1.7, y: y + 0.08, w: 3.2, h: 0.2,
      fontSize: 10, fontFace: "Microsoft YaHei",
      color: theme.primary, valign: "middle"
    });
  });

  // 动态管理命令
  slide.addShape(pres.shapes.ROUNDED_RECTANGLE, {
    x: 0.8, y: 3.95, w: 4.0, h: 0.85,
    fill: { color: "F0FDF9" }, line: { color: theme.accent, width: 0.5 },
    rectRadius: 0.04
  });
  slide.addText("动态管理命令", {
    x: 0.95, y: 3.98, w: 3.5, h: 0.25,
    fontSize: 10, fontFace: "Microsoft YaHei",
    color: theme.accent, bold: true
  });
  const cmds = ["acl add", "acl del", "acl clear", "acl list", "acl hits"];
  cmds.forEach((c, i) => {
    slide.addShape(pres.shapes.ROUNDED_RECTANGLE, {
      x: 0.95 + (i % 3) * 1.25, y: 4.25 + Math.floor(i / 3) * 0.25, w: 1.15, h: 0.22,
      fill: { color: theme.accent }, rectRadius: 0.03
    });
    slide.addText(c, {
      x: 0.95 + (i % 3) * 1.25, y: 4.25 + Math.floor(i / 3) * 0.25, w: 1.15, h: 0.22,
      fontSize: 8, fontFace: "Arial",
      color: "FFFFFF", bold: true, align: "center", valign: "middle"
    });
  });

  // 右侧：会话跟踪
  slide.addShape(pres.shapes.RECTANGLE, {
    x: 5.2, y: 1.3, w: 4.2, h: 3.65,
    fill: { color: "FFFFFF" }, line: { color: theme.light, width: 1 }
  });
  slide.addShape(pres.shapes.RECTANGLE, {
    x: 5.2, y: 1.3, w: 4.2, h: 0.5,
    fill: { color: theme.accent2 }
  });
  slide.addText("状态化连接跟踪", {
    x: 5.3, y: 1.3, w: 4.0, h: 0.5,
    fontSize: 15, fontFace: "Microsoft YaHei",
    color: "FFFFFF", bold: true, valign: "middle"
  });

  // 数据结构
  slide.addText("数据结构", {
    x: 5.4, y: 1.95, w: 2, h: 0.25,
    fontSize: 11, fontFace: "Microsoft YaHei",
    color: theme.primary, bold: true
  });
  slide.addText("DPDK rte_hash · 5元组键", {
    x: 5.4, y: 2.2, w: 4, h: 0.25,
    fontSize: 10, fontFace: "Microsoft YaHei",
    color: theme.accent2
  });
  slide.addText("查找复杂度 O(1) · per-lcore独立实例", {
    x: 5.4, y: 2.45, w: 4, h: 0.25,
    fontSize: 9, fontFace: "Microsoft YaHei",
    color: theme.secondary
  });

  // TCP 状态机
  slide.addText("TCP 状态机跟踪", {
    x: 5.4, y: 2.85, w: 2, h: 0.25,
    fontSize: 11, fontFace: "Microsoft YaHei",
    color: theme.primary, bold: true
  });

  // 状态机时间线
  const states = ["SYN", "ESTABLISHED", "FIN/RST", "TIME_WAIT"];
  states.forEach((s, i) => {
    const x = 5.4 + i * 0.95;
    slide.addShape(pres.shapes.ROUNDED_RECTANGLE, {
      x: x, y: 3.15, w: 0.85, h: 0.3,
      fill: { color: i === 0 || i === 2 ? "FEE2E2" : "DCFCE7" },
      line: { color: theme.primary, width: 0.5 },
      rectRadius: 0.04
    });
    slide.addText(s, {
      x: x, y: 3.15, w: 0.85, h: 0.3,
      fontSize: 8, fontFace: "Arial",
      color: theme.primary, bold: true,
      align: "center", valign: "middle"
    });
    if (i < states.length - 1) {
      slide.addText("→", {
        x: x + 0.85, y: 3.15, w: 0.1, h: 0.3,
        fontSize: 10, fontFace: "Arial",
        color: theme.secondary, align: "center", valign: "middle"
      });
    }
  });

  // UDP
  slide.addText("UDP 伪状态跟踪", {
    x: 5.4, y: 3.6, w: 4, h: 0.25,
    fontSize: 11, fontFace: "Microsoft YaHei",
    color: theme.primary, bold: true
  });
  slide.addText("无连接 → 基于五元组+超时机制伪跟踪", {
    x: 5.4, y: 3.85, w: 4, h: 0.25,
    fontSize: 9, fontFace: "Microsoft YaHei",
    color: theme.secondary
  });

  // 容量
  slide.addShape(pres.shapes.ROUNDED_RECTANGLE, {
    x: 5.4, y: 4.2, w: 3.8, h: 0.6,
    fill: { color: "FFF7ED" }, line: { color: theme.accent2, width: 0.5 },
    rectRadius: 0.04
  });
  slide.addText([
    { text: "40万", options: { fontSize: 22, bold: true, color: theme.accent2 } },
    { text: "  并发会话容量", options: { fontSize: 11, color: theme.primary } }
  ], {
    x: 5.4, y: 4.2, w: 3.8, h: 0.6,
    fontFace: "Microsoft YaHei", align: "center", valign: "middle"
  });

  // 底部
  slide.addShape(pres.shapes.ROUNDED_RECTANGLE, {
    x: 0.6, y: 5.0, w: 8.8, h: 0.4,
    fill: { color: theme.primary }, rectRadius: 0.05
  });
  slide.addText([
    { text: "IPv4/IPv6 双栈：", options: { color: theme.accent2, bold: true } },
    { text: "统一ACL + 独立session6会话表 + 共享per-lcore隔离策略", options: { color: "FFFFFF" } }
  ], {
    x: 0.6, y: 5.0, w: 8.8, h: 0.4,
    fontSize: 11, fontFace: "Microsoft YaHei",
    align: "center", valign: "middle"
  });

  slide.addText("08", {
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
  pres.writeFile({ fileName: "slide-08-preview.pptx" });
}

module.exports = { createSlide, slideConfig };
