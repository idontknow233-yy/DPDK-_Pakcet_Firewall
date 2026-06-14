// slide-07.js - 05 核心创新时间线
const pptxgen = require("pptxgenjs");

const slideConfig = { type: 'content', index: 7, title: '05 核心创新时间线' };

function createSlide(pres, theme) {
  const slide = pres.addSlide();
  slide.background = { color: theme.bg };

  slide.addText("05  /  核心创新", {
    x: 0.6, y: 0.3, w: 5, h: 0.3,
    fontSize: 11, fontFace: "Arial",
    color: theme.accent, bold: true, charSpacing: 3
  });

  slide.addText("数据包处理全流程技术创新时间线", {
    x: 0.6, y: 0.6, w: 9, h: 0.45,
    fontSize: 22, fontFace: "Microsoft YaHei",
    color: theme.primary, bold: true
  });

  // 主时间线水平线
  const timelineY = 1.85;
  slide.addShape(pres.shapes.LINE, {
    x: 0.6, y: timelineY, w: 8.8, h: 0,
    line: { color: theme.accent, width: 3 }
  });

  // 起点圆
  slide.addShape(pres.shapes.OVAL, {
    x: 0.45, y: timelineY - 0.1, w: 0.2, h: 0.2,
    fill: { color: theme.accent2 }
  });
  // 终点圆
  slide.addShape(pres.shapes.OVAL, {
    x: 9.35, y: timelineY - 0.1, w: 0.2, h: 0.2,
    fill: { color: theme.accent2 }
  });

  // 7个创新点
  const innovations = [
    { stage: "收包", title: "批量收包", tech: "rte_eth_rx_burst\n32包/批", desc: "摊薄函数调用开销", x: 0.5 },
    { stage: "检测", title: "攻击检测", tech: "端口扫描\n封禁统计", desc: "SYN/UDP Flood 实时感知", x: 1.85 },
    { stage: "限速", title: "令牌桶限速", tech: "SYN PPS\nUDP Burst", desc: "可配置速率限制策略", x: 3.2 },
    { stage: "ACL", title: "双缓冲无锁", tech: "ctx[0]/ctx[1]\n原子切换", desc: "数据路径始终无锁读", x: 4.55, highlight: true },
    { stage: "会话", title: "rte_hash O(1)", tech: "per-lcore\n独立哈希表", desc: "消除跨核缓存争用", x: 5.9, highlight: true },
    { stage: "路由", title: "NDP挂起队列", tech: "MAC未知时\n暂存数据包", desc: "解析完成后批量转发", x: 7.25, highlight: true },
    { stage: "发包", title: "TX缓冲排空", tech: "~100μs\n定时flush", desc: "批量发送降低系统调用", x: 8.6 }
  ];

  innovations.forEach((iv) => {
    const isUp = innovations.indexOf(iv) % 2 === 0;

    // 主节点圆
    const dotColor = iv.highlight ? theme.accent2 : theme.accent;
    const dotSize = iv.highlight ? 0.32 : 0.26;
    slide.addShape(pres.shapes.OVAL, {
      x: iv.x + 0.55 - dotSize/2, y: timelineY - dotSize/2, w: dotSize, h: dotSize,
      fill: { color: dotColor }
    });

    // 阶段标签
    slide.addText(iv.stage, {
      x: iv.x, y: timelineY - (isUp ? 0.4 : 0.15), w: 1.1, h: 0.25,
      fontSize: 9, fontFace: "Microsoft YaHei",
      color: dotColor, bold: true, align: "center"
    });

    if (isUp) {
      // 连接线向上
      slide.addShape(pres.shapes.LINE, {
        x: iv.x + 0.55, y: timelineY - 0.85, w: 0, h: 0.5,
        line: { color: theme.light, width: 1, dashType: "dash" }
      });
      // 上方卡片
      slide.addShape(pres.shapes.ROUNDED_RECTANGLE, {
        x: iv.x, y: 0.95, w: 1.1, h: 0.85,
        fill: { color: iv.highlight ? "FFF7ED" : "FFFFFF" },
        line: { color: dotColor, width: iv.highlight ? 1.5 : 0.5 },
        rectRadius: 0.04
      });
      slide.addText(iv.title, {
        x: iv.x, y: 0.97, w: 1.1, h: 0.22,
        fontSize: 10, fontFace: "Microsoft YaHei",
        color: theme.primary, bold: true, align: "center"
      });
      slide.addText(iv.tech, {
        x: iv.x, y: 1.2, w: 1.1, h: 0.4,
        fontSize: 8, fontFace: "Microsoft YaHei",
        color: dotColor, align: "center", lineSpacingMultiple: 1.1
      });
      slide.addText(iv.desc, {
        x: iv.x, y: 1.55, w: 1.1, h: 0.25,
        fontSize: 7, fontFace: "Microsoft YaHei",
        color: theme.secondary, align: "center"
      });
    } else {
      // 连接线向下
      slide.addShape(pres.shapes.LINE, {
        x: iv.x + 0.55, y: timelineY + 0.35, w: 0, h: 0.5,
        line: { color: theme.light, width: 1, dashType: "dash" }
      });
      // 下方卡片
      slide.addShape(pres.shapes.ROUNDED_RECTANGLE, {
        x: iv.x, y: 2.85, w: 1.1, h: 0.85,
        fill: { color: iv.highlight ? "FFF7ED" : "FFFFFF" },
        line: { color: dotColor, width: iv.highlight ? 1.5 : 0.5 },
        rectRadius: 0.04
      });
      slide.addText(iv.title, {
        x: iv.x, y: 2.87, w: 1.1, h: 0.22,
        fontSize: 10, fontFace: "Microsoft YaHei",
        color: theme.primary, bold: true, align: "center"
      });
      slide.addText(iv.tech, {
        x: iv.x, y: 3.1, w: 1.1, h: 0.4,
        fontSize: 8, fontFace: "Microsoft YaHei",
        color: dotColor, align: "center", lineSpacingMultiple: 1.1
      });
      slide.addText(iv.desc, {
        x: iv.x, y: 3.45, w: 1.1, h: 0.25,
        fontSize: 7, fontFace: "Microsoft YaHei",
        color: theme.secondary, align: "center"
      });
    }
  });

  // 底部：核心创新说明
  slide.addShape(pres.shapes.ROUNDED_RECTANGLE, {
    x: 0.6, y: 4.0, w: 8.8, h: 1.3,
    fill: { color: theme.primary }, rectRadius: 0.06
  });
  slide.addShape(pres.shapes.RECTANGLE, {
    x: 0.6, y: 4.0, w: 0.12, h: 1.3,
    fill: { color: theme.accent2 }
  });
  slide.addText("3 大核心创新点", {
    x: 0.9, y: 4.1, w: 4, h: 0.3,
    fontSize: 13, fontFace: "Microsoft YaHei",
    color: theme.accent2, bold: true
  });

  const cores = [
    { t: "ACL双缓冲", d: "ctx[0]/ctx[1]双份上下文 + 原子切换，读路径零阻塞" },
    { t: "Per-lcore隔离", d: "每核独立hash/scan/限速表，消除跨核缓存行争用" },
    { t: "NDP挂起队列", d: "MAC未知时挂起包，解析完成后批量转发，避免丢包" }
  ];

  cores.forEach((c, i) => {
    const x = 0.9 + i * 2.85;
    slide.addText(c.t, {
      x: x, y: 4.5, w: 2.7, h: 0.3,
      fontSize: 12, fontFace: "Microsoft YaHei",
      color: "FFFFFF", bold: true
    });
    slide.addText(c.d, {
      x: x, y: 4.78, w: 2.7, h: 0.5,
      fontSize: 9, fontFace: "Microsoft YaHei",
      color: theme.light
    });
  });

  // 页码
  slide.addText("07", {
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
  pres.writeFile({ fileName: "slide-07-preview.pptx" });
}

module.exports = { createSlide, slideConfig };
