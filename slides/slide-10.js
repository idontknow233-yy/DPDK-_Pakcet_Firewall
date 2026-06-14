// slide-10.js - 08 控制面 CLI
const pptxgen = require("pptxgenjs");

const slideConfig = { type: 'content', index: 10, title: '08 控制面CLI' };

function createSlide(pres, theme) {
  const slide = pres.addSlide();
  slide.background = { color: theme.bg };

  slide.addText("08  /  管理接口", {
    x: 0.6, y: 0.35, w: 5, h: 0.3,
    fontSize: 11, fontFace: "Arial",
    color: theme.accent, bold: true, charSpacing: 3
  });

  slide.addText("控制面 CLI 命令系统", {
    x: 0.6, y: 0.65, w: 9, h: 0.5,
    fontSize: 24, fontFace: "Microsoft YaHei",
    color: theme.primary, bold: true
  });

  // 左侧：CLI终端模拟
  slide.addShape(pres.shapes.ROUNDED_RECTANGLE, {
    x: 0.6, y: 1.3, w: 4.2, h: 3.65,
    fill: { color: "0F172A" }, rectRadius: 0.06
  });
  // 终端标题栏
  slide.addShape(pres.shapes.ROUNDED_RECTANGLE, {
    x: 0.6, y: 1.3, w: 4.2, h: 0.35,
    fill: { color: "1E293B" }, rectRadius: 0.06
  });
  // 三个圆点
  slide.addShape(pres.shapes.OVAL, { x: 0.75, y: 1.4, w: 0.15, h: 0.15, fill: { color: "EF4444" } });
  slide.addShape(pres.shapes.OVAL, { x: 0.95, y: 1.4, w: 0.15, h: 0.15, fill: { color: "F59E0B" } });
  slide.addShape(pres.shapes.OVAL, { x: 1.15, y: 1.4, w: 0.15, h: 0.15, fill: { color: "10B981" } });
  slide.addText("fw-cli — nc 127.0.0.1 8086", {
    x: 1.4, y: 1.32, w: 3.2, h: 0.32,
    fontSize: 9, fontFace: "Arial",
    color: "94A3B8", valign: "middle"
  });

  // 终端内容
  const lines = [
    { p: "$ ", t: "acl add allow 192.168.1.0/24 any", c: theme.accent },
    { p: "  ", t: "[OK] rule idx=1, version=2", c: "94A3B8" },
    { p: "$ ", t: "acl add deny proto=17", c: theme.accent },
    { p: "  ", t: "[OK] rule idx=2, version=3", c: "94A3B8" },
    { p: "$ ", t: "acl list", c: theme.accent },
    { p: "  ", t: "# idx prio  src          dst     act", c: "64748B" },
    { p: "  ", t: "  1   10   192.168.1.0/24 any    allow", c: "FFFFFF" },
    { p: "  ", t: "  2   20   any          any     deny", c: "FFFFFF" },
    { p: "$ ", t: "port stats", c: theme.accent },
    { p: "  ", t: "Port 0: RX=1.2Mpps  TX=1.1Mpps  drop=0", c: "94A3B8" },
    { p: "$ ", t: "session list 5", c: theme.accent },
    { p: "  ", t: "[ESTABLISHED] 10.0.0.1:80  10.0.0.2:1234", c: "FFFFFF" },
    { p: "$ ", t: "_", c: theme.accent }
  ];

  lines.forEach((l, i) => {
    slide.addText([
      { text: l.p, options: { color: theme.accent2, bold: true } },
      { text: l.t, options: { color: l.c } }
    ], {
      x: 0.8, y: 1.75 + i * 0.22, w: 3.9, h: 0.22,
      fontSize: 9, fontFace: "Consolas",
      valign: "middle"
    });
  });

  // 右侧：命令分类
  slide.addText("25+ 条 CLI 命令 · 9 大类", {
    x: 5.1, y: 1.3, w: 4.3, h: 0.3,
    fontSize: 14, fontFace: "Microsoft YaHei",
    color: theme.primary, bold: true
  });
  slide.addText("基于 DPDK rte_cmdline 库 · TCP 8086", {
    x: 5.1, y: 1.6, w: 4.3, h: 0.25,
    fontSize: 10, fontFace: "Microsoft YaHei",
    color: theme.secondary
  });

  const cmdGroups = [
    { c: "acl", d: "IPv4 ACL规则CRUD + 命中统计" },
    { c: "acl6", d: "IPv6 ACL规则CRUD" },
    { c: "session", d: "IPv4连接表查询" },
    { c: "session6", d: "IPv6连接表查询" },
    { c: "route / route6", d: "IPv4/IPv6 路由管理" },
    { c: "port", d: "端口收发包统计" },
    { c: "ifcfg4", d: "IPv4端口配置" },
    { c: "deny / deny6", d: "IPv4/IPv6 拒绝日志" },
    { c: "attack / ddos", d: "攻击防御 + 限速配置" }
  ];

  cmdGroups.forEach((g, i) => {
    const y = 1.95 + i * 0.32;
    slide.addShape(pres.shapes.RECTANGLE, {
      x: 5.1, y: y, w: 1.4, h: 0.26,
      fill: { color: theme.primary }
    });
    slide.addText(g.c, {
      x: 5.1, y: y, w: 1.4, h: 0.26,
      fontSize: 10, fontFace: "Consolas",
      color: theme.accent2, bold: true, align: "center", valign: "middle"
    });
    slide.addText(g.d, {
      x: 6.6, y: y, w: 2.8, h: 0.26,
      fontSize: 9, fontFace: "Microsoft YaHei",
      color: theme.secondary, valign: "middle"
    });
  });

  slide.addText("10", {
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
  pres.writeFile({ fileName: "slide-10-preview.pptx" });
}

module.exports = { createSlide, slideConfig };
