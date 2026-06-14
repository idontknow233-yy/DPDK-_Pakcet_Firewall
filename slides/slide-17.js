// slide-17.js - 15 总结与展望
const pptxgen = require("pptxgenjs");

const slideConfig = { type: 'content', index: 17, title: '15 总结与展望' };

function createSlide(pres, theme) {
  const slide = pres.addSlide();
  slide.background = { color: theme.bg };

  slide.addText("15  /  总结与展望", {
    x: 0.6, y: 0.35, w: 5, h: 0.3,
    fontSize: 11, fontFace: "Arial",
    color: theme.accent, bold: true, charSpacing: 3
  });

  slide.addText("总结与未来展望", {
    x: 0.6, y: 0.65, w: 9, h: 0.5,
    fontSize: 24, fontFace: "Microsoft YaHei",
    color: theme.primary, bold: true
  });

  // 左侧：工作总结
  slide.addShape(pres.shapes.RECTANGLE, {
    x: 0.5, y: 1.3, w: 4.4, h: 3.65,
    fill: { color: "FFFFFF" }, line: { color: theme.light, width: 1 }
  });
  slide.addShape(pres.shapes.RECTANGLE, {
    x: 0.5, y: 1.3, w: 4.4, h: 0.45,
    fill: { color: theme.accent }
  });
  slide.addText("📋  工作总结", {
    x: 0.7, y: 1.3, w: 4.1, h: 0.45,
    fontSize: 14, fontFace: "Microsoft YaHei",
    color: "FFFFFF", bold: true, valign: "middle"
  });

  const summary = [
    { t: "高性能数据面", d: "基于 DPDK 实现线速数据包处理流水线" },
    { t: "完整双栈支持", d: "IPv4/IPv6 + 五元组ACL + 状态化跟踪 + LPM路由" },
    { t: "三层 IPC 机制", d: "共享内存 + 无锁环 + 双缓冲 + 版本号" },
    { t: "完整管理链", d: "Vue3 + Go + C 控制/数据平面全栈打通" },
    { t: "Per-lcore 无锁", d: "验证 DPDK 在高速网络环境下的可行性" }
  ];

  summary.forEach((s, i) => {
    const y = 1.95 + i * 0.58;
    slide.addShape(pres.shapes.RECTANGLE, {
      x: 0.7, y: y, w: 0.05, h: 0.5,
      fill: { color: theme.accent2 }
    });
    slide.addText(s.t, {
      x: 0.85, y: y, w: 3.9, h: 0.25,
      fontSize: 12, fontFace: "Microsoft YaHei",
      color: theme.primary, bold: true
    });
    slide.addText(s.d, {
      x: 0.85, y: y + 0.25, w: 3.9, h: 0.25,
      fontSize: 9, fontFace: "Microsoft YaHei",
      color: theme.secondary
    });
  });

  // 右侧：未来展望
  slide.addShape(pres.shapes.RECTANGLE, {
    x: 5.1, y: 1.3, w: 4.3, h: 3.65,
    fill: { color: "FFFFFF" }, line: { color: theme.light, width: 1 }
  });
  slide.addShape(pres.shapes.RECTANGLE, {
    x: 5.1, y: 1.3, w: 4.3, h: 0.45,
    fill: { color: theme.accent2 }
  });
  slide.addText("🚀  未来展望", {
    x: 5.3, y: 1.3, w: 4.0, h: 0.45,
    fontSize: 14, fontFace: "Microsoft YaHei",
    color: "FFFFFF", bold: true, valign: "middle"
  });

  const future = [
    { icon: "📈", t: "ACL 大规模优化", d: "集成 rte_acl (HyperSplit) 提升千条规则性能" },
    { icon: "🔍", t: "DPI 深度包检测", d: "支持应用层协议识别" },
    { icon: "🧠", t: "NUMA 感知", d: "多 NUMA 节点内存分配优化" },
    { icon: "⚡", t: "eBPF/XDP 加速", d: "辅助数据平面加速" },
    { icon: "🔌", t: "高速网卡支持", d: "i40e / mlx5 等 10G/40G 网卡" }
  ];

  future.forEach((f, i) => {
    const y = 1.95 + i * 0.58;
    slide.addShape(pres.shapes.OVAL, {
      x: 5.3, y: y + 0.05, w: 0.4, h: 0.4,
      fill: { color: "FFF7ED" }
    });
    slide.addText(f.icon, {
      x: 5.3, y: y + 0.05, w: 0.4, h: 0.4,
      fontSize: 14, fontFace: "Arial",
      align: "center", valign: "middle"
    });
    slide.addText(f.t, {
      x: 5.8, y: y, w: 3.4, h: 0.25,
      fontSize: 12, fontFace: "Microsoft YaHei",
      color: theme.primary, bold: true
    });
    slide.addText(f.d, {
      x: 5.8, y: y + 0.25, w: 3.4, h: 0.25,
      fontSize: 9, fontFace: "Microsoft YaHei",
      color: theme.secondary
    });
  });

  // 底部
  slide.addShape(pres.shapes.ROUNDED_RECTANGLE, {
    x: 0.5, y: 5.0, w: 8.9, h: 0.4,
    fill: { color: theme.primary }, rectRadius: 0.05
  });
  slide.addText("感谢各位老师的聆听与指导  ·  欢迎批评指正", {
    x: 0.5, y: 5.0, w: 8.9, h: 0.4,
    fontSize: 12, fontFace: "Microsoft YaHei",
    color: "FFFFFF", bold: true, align: "center", valign: "middle"
  });

  slide.addText("17", {
    x: 9.3, y: 5.1, w: 0.4, h: 0.3,
    fontSize: 11, fontFace: "Arial",
    color: "FFFFFF", align: "right"
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
  pres.writeFile({ fileName: "slide-17-preview.pptx" });
}

module.exports = { createSlide, slideConfig };
