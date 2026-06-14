// slide-12.js - 10 性能优化 7 要点
const pptxgen = require("pptxgenjs");

const slideConfig = { type: 'content', index: 12, title: '10 性能优化7要点' };

function createSlide(pres, theme) {
  const slide = pres.addSlide();
  slide.background = { color: theme.bg };

  slide.addText("10  /  性能优化", {
    x: 0.6, y: 0.35, w: 5, h: 0.3,
    fontSize: 11, fontFace: "Arial",
    color: theme.accent, bold: true, charSpacing: 3
  });

  slide.addText("性能优化设计 7 要点", {
    x: 0.6, y: 0.65, w: 9, h: 0.5,
    fontSize: 24, fontFace: "Microsoft YaHei",
    color: theme.primary, bold: true
  });

  // 7个优化要点 (3+4 网格)
  const opts = [
    { n: "01", t: "批量收发包", d: "rte_eth_rx_burst 32包/次", d2: "TX 缓冲 ~100μs 排空" },
    { n: "02", t: "Per-lcore 隔离", d: "每核独立会话/扫描/限速表", d2: "零跨核缓存行争用" },
    { n: "03", t: "ACL 双缓冲", d: "ctx[0] / ctx[1] 双份上下文", d2: "读路径始终无锁" },
    { n: "04", t: "DPDK LPM", d: "Dir-24-8 算法", d2: "O(1) 最长前缀匹配" },
    { n: "05", t: "rte_hash", d: "高速哈希会话表", d2: "per-lcore 实例化" },
    { n: "06", t: "无锁环队列", d: "rte_ring MP/SC 模式", d2: "ACL 命令/响应零开销" },
    { n: "07", t: "版本号", d: "rte_atomic64_t 共享配置", d2: "wmb/rmb 一致性保证" }
  ];

  const cardW = 2.85, cardH = 1.65;
  const startX = 0.5, startY = 1.3, gapX = 0.15, gapY = 0.15;

  opts.forEach((o, i) => {
    const col = i % 3;
    const row = Math.floor(i / 3);
    const x = startX + col * (cardW + gapX);
    const y = startY + row * (cardH + gapY);

    // 卡片背景
    slide.addShape(pres.shapes.ROUNDED_RECTANGLE, {
      x: x, y: y, w: cardW, h: cardH,
      fill: { color: "FFFFFF" }, line: { color: theme.light, width: 1 },
      rectRadius: 0.05
    });
    // 顶部色条
    slide.addShape(pres.shapes.RECTANGLE, {
      x: x, y: y, w: cardW, h: 0.08,
      fill: { color: theme.accent }
    });

    // 编号
    slide.addText(o.n, {
      x: x + 0.15, y: y + 0.15, w: 0.7, h: 0.4,
      fontSize: 22, fontFace: "Arial",
      color: theme.accent2, bold: true
    });

    // 标题
    slide.addText(o.t, {
      x: x + 0.85, y: y + 0.2, w: cardW - 1.0, h: 0.35,
      fontSize: 14, fontFace: "Microsoft YaHei",
      color: theme.primary, bold: true, valign: "middle"
    });

    // 分隔线
    slide.addShape(pres.shapes.LINE, {
      x: x + 0.15, y: y + 0.7, w: cardW - 0.3, h: 0,
      line: { color: theme.light, width: 0.5 }
    });

    // 描述 1
    slide.addText(o.d, {
      x: x + 0.15, y: y + 0.8, w: cardW - 0.3, h: 0.3,
      fontSize: 10, fontFace: "Microsoft YaHei",
      color: theme.primary
    });

    // 描述 2
    slide.addText(o.d2, {
      x: x + 0.15, y: y + 1.1, w: cardW - 0.3, h: 0.3,
      fontSize: 9, fontFace: "Microsoft YaHei",
      color: theme.secondary
    });

    // 底部角标
    slide.addText(`OPT-${o.n}`, {
      x: x + cardW - 0.85, y: y + cardH - 0.3, w: 0.7, h: 0.2,
      fontSize: 7, fontFace: "Arial",
      color: theme.light, align: "right"
    });
  });

  // 底部
  slide.addText("12", {
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
  pres.writeFile({ fileName: "slide-12-preview.pptx" });
}

module.exports = { createSlide, slideConfig };
