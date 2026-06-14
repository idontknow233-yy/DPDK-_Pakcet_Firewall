// slide-14.js - 12 功能测试
const pptxgen = require("pptxgenjs");

const slideConfig = { type: 'content', index: 14, title: '12 功能测试' };

function createSlide(pres, theme) {
  const slide = pres.addSlide();
  slide.background = { color: theme.bg };

  slide.addText("12  /  功能测试", {
    x: 0.6, y: 0.35, w: 5, h: 0.3,
    fontSize: 11, fontFace: "Arial",
    color: theme.accent, bold: true, charSpacing: 3
  });

  slide.addText("功能测试结果", {
    x: 0.6, y: 0.65, w: 9, h: 0.5,
    fontSize: 24, fontFace: "Microsoft YaHei",
    color: theme.primary, bold: true
  });

  // 4个测试类别
  const testGroups = [
    {
      icon: "🛡", n: "1", t: "数据包过滤", color: theme.accent,
      items: [
        "默认allow-all / deny src / deny dst-port / deny proto",
        "allow 优先级高于 deny any",
        "规则按添加顺序匹配，先匹配先命中",
        "命中规则在Web拒绝日志有记录"
      ]
    },
    {
      icon: "🔗", n: "2", t: "连接跟踪", color: "3B82F6",
      items: [
        "TCP 三次握手 → ESTABLISHED 状态正确",
        "TCP 四次挥手 → 会话条目被清除",
        "TCP RST 复位 → 会话条目立即清除",
        "UDP 流 → 伪状态跟踪 + 300s 超时清理"
      ]
    },
    {
      icon: "💻", n: "3", t: "CLI 命令", color: theme.accent2,
      items: [
        "acl add/del/clear/list/hits 全部正确",
        "port stats 显示 RX/TX/drop 计数 + 链路",
        "deny list / session list / ddos show 正常",
        "配置下发后数据面行为与预期一致"
      ]
    },
    {
      icon: "🌐", n: "4", t: "Web 管理", color: "10B981",
      items: [
        "10 个页面数据展示 + 交互操作正常",
        "Dashboard 5s 自动轮询 5 个 API 端点",
        "ACL 规则增删前端→后端→数据面全链路生效",
        "ECharts 图表挂载初始化/卸载销毁正确"
      ]
    }
  ];

  const cardW = 4.4, cardH = 1.75;
  const startX = 0.5, startY = 1.3, gapX = 0.2, gapY = 0.15;

  testGroups.forEach((g, i) => {
    const col = i % 2;
    const row = Math.floor(i / 2);
    const x = startX + col * (cardW + gapX);
    const y = startY + row * (cardH + gapY);

    slide.addShape(pres.shapes.RECTANGLE, {
      x: x, y: y, w: cardW, h: cardH,
      fill: { color: "FFFFFF" }, line: { color: theme.light, width: 1 }
    });
    // 左侧色条
    slide.addShape(pres.shapes.RECTANGLE, {
      x: x, y: y, w: 0.1, h: cardH,
      fill: { color: g.color }
    });
    // 标题区
    slide.addText(g.icon + "  " + g.t, {
      x: x + 0.2, y: y + 0.1, w: 3, h: 0.3,
      fontSize: 14, fontFace: "Microsoft YaHei",
      color: theme.primary, bold: true
    });
    // 编号
    slide.addShape(pres.shapes.OVAL, {
      x: x + cardW - 0.5, y: y + 0.1, w: 0.35, h: 0.35,
      fill: { color: g.color }
    });
    slide.addText(g.n, {
      x: x + cardW - 0.5, y: y + 0.1, w: 0.35, h: 0.35,
      fontSize: 14, fontFace: "Arial",
      color: "FFFFFF", bold: true,
      align: "center", valign: "middle"
    });

    // 测试项列表
    g.items.forEach((item, j) => {
      slide.addShape(pres.shapes.OVAL, {
        x: x + 0.25, y: y + 0.55 + j * 0.27 + 0.07, w: 0.08, h: 0.08,
        fill: { color: g.color }
      });
      slide.addText(item, {
        x: x + 0.4, y: y + 0.52 + j * 0.27, w: cardW - 0.5, h: 0.25,
        fontSize: 9, fontFace: "Microsoft YaHei",
        color: theme.secondary, valign: "middle"
      });
    });
  });

  // 底部：通过标记
  slide.addShape(pres.shapes.ROUNDED_RECTANGLE, {
    x: 0.5, y: 5.0, w: 8.9, h: 0.4,
    fill: { color: "10B981" }, rectRadius: 0.05
  });
  slide.addText([
    { text: "✓ ", options: { color: "FFFFFF", bold: true, fontSize: 14 } },
    { text: "全部 27 个测试用例通过  ·  ", options: { color: "FFFFFF", bold: true } },
    { text: "ACL 7  ·  会话 4  ·  CLI 7  ·  Web 9", options: { color: "FFFFFF" } }
  ], {
    x: 0.5, y: 5.0, w: 8.9, h: 0.4,
    fontSize: 11, fontFace: "Microsoft YaHei",
    align: "center", valign: "middle"
  });

  slide.addText("14", {
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
  pres.writeFile({ fileName: "slide-14-preview.pptx" });
}

module.exports = { createSlide, slideConfig };
