// slide-16.js - 14 系统亮点与不足
const pptxgen = require("pptxgenjs");

const slideConfig = { type: 'content', index: 16, title: '14 系统亮点与不足' };

function createSlide(pres, theme) {
  const slide = pres.addSlide();
  slide.background = { color: theme.bg };

  slide.addText("14  /  总结与展望", {
    x: 0.6, y: 0.35, w: 5, h: 0.3,
    fontSize: 11, fontFace: "Arial",
    color: theme.accent, bold: true, charSpacing: 3
  });

  slide.addText("系统亮点与不足", {
    x: 0.6, y: 0.65, w: 9, h: 0.5,
    fontSize: 24, fontFace: "Microsoft YaHei",
    color: theme.primary, bold: true
  });

  // 左侧：5大亮点
  slide.addShape(pres.shapes.RECTANGLE, {
    x: 0.5, y: 1.3, w: 5.6, h: 3.65,
    fill: { color: "FFFFFF" }, line: { color: theme.light, width: 1 }
  });
  slide.addShape(pres.shapes.RECTANGLE, {
    x: 0.5, y: 1.3, w: 5.6, h: 0.45,
    fill: { color: theme.accent }
  });
  slide.addText("✓  系统亮点", {
    x: 0.7, y: 1.3, w: 5.3, h: 0.45,
    fontSize: 14, fontFace: "Microsoft YaHei",
    color: "FFFFFF", bold: true, valign: "middle"
  });

  const highlights = [
    { t: "数据面/控制面分离", d: "DPDK PRIMARY/SECONDARY 进程解耦，独立演进" },
    { t: "IPv4/IPv6 双栈", d: "统一流水线，完整双栈覆盖" },
    { t: "状态化连接跟踪", d: "rte_hash O(1)查找 + TCP/UDP 全状态跟踪" },
    { t: "Web + CLI 双管理接口", d: "Vue3 + 17 REST API + 25 项 CLI 命令" },
    { t: "配置持久化", d: "SQLite 数据库 + 启动时自动同步规则" },
    { t: "无锁快速路径", d: "per-lcore 数据隔离 + 零拷贝批量处理" }
  ];

  highlights.forEach((h, i) => {
    const y = 1.95 + i * 0.48;
    // 圆圈 + 勾
    slide.addShape(pres.shapes.OVAL, {
      x: 0.75, y: y + 0.05, w: 0.3, h: 0.3,
      fill: { color: theme.accent }
    });
    slide.addText("✓", {
      x: 0.75, y: y + 0.05, w: 0.3, h: 0.3,
      fontSize: 11, fontFace: "Arial",
      color: "FFFFFF", bold: true,
      align: "center", valign: "middle"
    });
    slide.addText(h.t, {
      x: 1.15, y: y, w: 4.8, h: 0.22,
      fontSize: 11, fontFace: "Microsoft YaHei",
      color: theme.primary, bold: true
    });
    slide.addText(h.d, {
      x: 1.15, y: y + 0.22, w: 4.8, h: 0.22,
      fontSize: 9, fontFace: "Microsoft YaHei",
      color: theme.secondary
    });
  });

  // 右侧：3点不足
  slide.addShape(pres.shapes.RECTANGLE, {
    x: 6.3, y: 1.3, w: 3.1, h: 3.65,
    fill: { color: "FFFFFF" }, line: { color: theme.light, width: 1 }
  });
  slide.addShape(pres.shapes.RECTANGLE, {
    x: 6.3, y: 1.3, w: 3.1, h: 0.45,
    fill: { color: "DC2626" }
  });
  slide.addText("!  存在的不足", {
    x: 6.5, y: 1.3, w: 2.8, h: 0.45,
    fontSize: 14, fontFace: "Microsoft YaHei",
    color: "FFFFFF", bold: true, valign: "middle"
  });

  const limits = [
    { t: "ACL 线性遍历", d: "千条以上规则性能线性下降，尚未集成 rte_acl" },
    { t: "IPv6 不完整", d: "扩展头部处理不全，NDP 实现较基础" },
    { t: "攻击检测简单", d: "仅基础SYN Flood检测，缺少更丰富检测算法" }
  ];

  limits.forEach((l, i) => {
    const y = 1.95 + i * 0.95;
    // 警告图标
    slide.addShape(pres.shapes.OVAL, {
      x: 6.5, y: y + 0.05, w: 0.3, h: 0.3,
      fill: { color: "FEE2E2" }
    });
    slide.addText("!", {
      x: 6.5, y: y + 0.05, w: 0.3, h: 0.3,
      fontSize: 12, fontFace: "Arial",
      color: "DC2626", bold: true,
      align: "center", valign: "middle"
    });
    slide.addText(l.t, {
      x: 6.9, y: y, w: 2.4, h: 0.25,
      fontSize: 11, fontFace: "Microsoft YaHei",
      color: theme.primary, bold: true
    });
    slide.addText(l.d, {
      x: 6.9, y: y + 0.25, w: 2.4, h: 0.55,
      fontSize: 9, fontFace: "Microsoft YaHei",
      color: theme.secondary, lineSpacingMultiple: 1.3
    });
  });

  // 底部
  slide.addShape(pres.shapes.ROUNDED_RECTANGLE, {
    x: 0.5, y: 5.0, w: 8.9, h: 0.4,
    fill: { color: theme.primary }, rectRadius: 0.05
  });
  slide.addText("相比传统内核防火墙，显著降低中断/拷贝开销，大包场景下达到线速处理能力", {
    x: 0.5, y: 5.0, w: 8.9, h: 0.4,
    fontSize: 11, fontFace: "Microsoft YaHei",
    color: "FFFFFF", align: "center", valign: "middle"
  });

  slide.addText("16", {
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
  pres.writeFile({ fileName: "slide-16-preview.pptx" });
}

module.exports = { createSlide, slideConfig };
