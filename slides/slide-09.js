// slide-09.js - 07 路由与 IPC 通信
const pptxgen = require("pptxgenjs");

const slideConfig = { type: 'content', index: 9, title: '07 路由与IPC通信' };

function createSlide(pres, theme) {
  const slide = pres.addSlide();
  slide.background = { color: theme.bg };

  slide.addText("07  /  核心模块", {
    x: 0.6, y: 0.35, w: 5, h: 0.3,
    fontSize: 11, fontFace: "Arial",
    color: theme.accent, bold: true, charSpacing: 3
  });

  slide.addText("路由查找与进程间通信机制", {
    x: 0.6, y: 0.65, w: 9, h: 0.5,
    fontSize: 24, fontFace: "Microsoft YaHei",
    color: theme.primary, bold: true
  });

  // 左侧：LPM路由图
  slide.addImage({
    path: "./imgs/IPv4快速路径.png",
    x: 0.4, y: 1.3, w: 2.5, h: 3.6
  });

  // 路由文字说明
  slide.addShape(pres.shapes.RECTANGLE, {
    x: 3.0, y: 1.3, w: 2.3, h: 3.6,
    fill: { color: "FFFFFF" }, line: { color: theme.light, width: 1 }
  });
  slide.addShape(pres.shapes.RECTANGLE, {
    x: 3.0, y: 1.3, w: 2.3, h: 0.4,
    fill: { color: theme.accent }
  });
  slide.addText("LPM 路由", {
    x: 3.1, y: 1.3, w: 2.1, h: 0.4,
    fontSize: 12, fontFace: "Microsoft YaHei",
    color: "FFFFFF", bold: true, valign: "middle"
  });

  const routeFeatures = [
    { t: "Dir-24-8 算法", d: "rte_lpm O(1)查找" },
    { t: "LPM 匹配", d: "最长前缀匹配路由表" },
    { t: "IPv4/IPv6 双栈", d: "统一查找接口" }
  ];
  routeFeatures.forEach((r, i) => {
    const y = 1.85 + i * 0.5;
    slide.addText(r.t, {
      x: 3.15, y: y, w: 2.0, h: 0.22,
      fontSize: 10, fontFace: "Microsoft YaHei",
      color: theme.primary, bold: true
    });
    slide.addText(r.d, {
      x: 3.15, y: y + 0.22, w: 2.0, h: 0.22,
      fontSize: 8, fontFace: "Microsoft YaHei",
      color: theme.secondary
    });
  });

  // ARP/ND
  slide.addShape(pres.shapes.ROUNDED_RECTANGLE, {
    x: 3.15, y: 3.4, w: 2.0, h: 1.4,
    fill: { color: "F0FDF9" }, line: { color: theme.accent, width: 0.5 },
    rectRadius: 0.04
  });
  slide.addText("ARP / NDP 解析", {
    x: 3.2, y: 3.45, w: 1.9, h: 0.25,
    fontSize: 10, fontFace: "Microsoft YaHei",
    color: theme.accent, bold: true
  });
  slide.addText([
    { text: "·  哈希表维护 IP→MAC 映射", options: { breakLine: true } },
    { text: "·  挂起队列暂存未知MAC包", options: { breakLine: true } },
    { text: "·  老化机制定期清理无效项", options: { breakLine: true } },
    { text: "·  IPv6使用NDP替代ARP", options: {} }
  ], {
    x: 3.2, y: 3.7, w: 1.9, h: 1.1,
    fontSize: 8, fontFace: "Microsoft YaHei",
    color: theme.secondary, lineSpacingMultiple: 1.3
  });

  // 右侧：IPC通信
  slide.addShape(pres.shapes.RECTANGLE, {
    x: 5.5, y: 1.3, w: 3.9, h: 3.6,
    fill: { color: "FFFFFF" }, line: { color: theme.light, width: 1 }
  });
  slide.addShape(pres.shapes.RECTANGLE, {
    x: 5.5, y: 1.3, w: 3.9, h: 0.4,
    fill: { color: theme.accent2 }
  });
  slide.addText("三层 IPC 通信机制", {
    x: 5.6, y: 1.3, w: 3.7, h: 0.4,
    fontSize: 13, fontFace: "Microsoft YaHei",
    color: "FFFFFF", bold: true, valign: "middle"
  });

  // 三个IPC层
  const ipcLayers = [
    {
      n: "L1", t: "共享内存", tech: "rte_memzone",
      desc: "15个共享结构体 · 配置/统计双向传输 · 只读零锁"
    },
    {
      n: "L2", t: "无锁队列", tech: "rte_ring",
      desc: "ACL命令/响应环 · MP/SC模式 · 1024容量"
    },
    {
      n: "L3", t: "版本号", tech: "rte_atomic64_t",
      desc: "wmb/rmb内存屏障 · 配置一致性保证"
    }
  ];

  ipcLayers.forEach((l, i) => {
    const y = 1.9 + i * 1.0;
    // 编号
    slide.addShape(pres.shapes.RECTANGLE, {
      x: 5.65, y: y, w: 0.5, h: 0.85,
      fill: { color: theme.accent2 }
    });
    slide.addText(l.n, {
      x: 5.65, y: y, w: 0.5, h: 0.85,
      fontSize: 14, fontFace: "Arial",
      color: "FFFFFF", bold: true,
      align: "center", valign: "middle"
    });
    // 内容
    slide.addText(l.t, {
      x: 6.25, y: y, w: 3.0, h: 0.28,
      fontSize: 13, fontFace: "Microsoft YaHei",
      color: theme.primary, bold: true
    });
    slide.addText(l.tech, {
      x: 6.25, y: y + 0.28, w: 3.0, h: 0.22,
      fontSize: 9, fontFace: "Arial",
      color: theme.accent2, bold: true
    });
    slide.addText(l.desc, {
      x: 6.25, y: y + 0.5, w: 3.0, h: 0.32,
      fontSize: 9, fontFace: "Microsoft YaHei",
      color: theme.secondary
    });
  });

  // 底部
  slide.addShape(pres.shapes.ROUNDED_RECTANGLE, {
    x: 0.6, y: 5.0, w: 8.8, h: 0.4,
    fill: { color: theme.primary }, rectRadius: 0.05
  });
  slide.addText([
    { text: "设计效果：", options: { color: theme.accent2, bold: true } },
    { text: "绕开复杂分布式锁  ·  保证一致性  ·  通信延迟压至最低  ·  控制面/数据面独立演进", options: { color: "FFFFFF" } }
  ], {
    x: 0.6, y: 5.0, w: 8.8, h: 0.4,
    fontSize: 11, fontFace: "Microsoft YaHei",
    align: "center", valign: "middle"
  });

  slide.addText("09", {
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
  pres.writeFile({ fileName: "slide-09-preview.pptx" });
}

module.exports = { createSlide, slideConfig };
