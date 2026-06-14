// slide-15.js - 13 性能测试 (柱状图 + 折线图)
const pptxgen = require("pptxgenjs");

const slideConfig = { type: 'content', index: 15, title: '13 性能测试' };

function createSlide(pres, theme) {
  const slide = pres.addSlide();
  slide.background = { color: theme.bg };

  slide.addText("13  /  性能测试", {
    x: 0.6, y: 0.35, w: 5, h: 0.3,
    fontSize: 11, fontFace: "Arial",
    color: theme.accent, bold: true, charSpacing: 3
  });

  slide.addText("性能测试结果", {
    x: 0.6, y: 0.65, w: 9, h: 0.5,
    fontSize: 24, fontFace: "Microsoft YaHei",
    color: theme.primary, bold: true
  });

  // 数据准备
  const labels = ["64B", "128B", "256B", "512B", "1024B", "1514B"];
  const lineRate = [1.488, 0.844, 0.452, 0.234, 0.119, 0.081];
  const throughput = [0.449, 0.411, 0.394, 0.285, 0.319, 0.262];
  const percent = [23.0, 42.1, 80.8, 100.0, 100.0, 100.0];

  // 左侧：柱状图 (吞吐量 vs 理论线速)
  slide.addShape(pres.shapes.RECTANGLE, {
    x: 0.4, y: 1.3, w: 4.8, h: 3.5,
    fill: { color: "FFFFFF" }, line: { color: theme.light, width: 1 }
  });
  slide.addText("吞吐量 vs 理论线速 (Mpps)", {
    x: 0.6, y: 1.4, w: 4, h: 0.3,
    fontSize: 12, fontFace: "Microsoft YaHei",
    color: theme.primary, bold: true
  });

  const chartDataBar = [
    {
      name: "理论线速",
      labels: labels,
      values: lineRate
    },
    {
      name: "实测吞吐量",
      labels: labels,
      values: throughput
    }
  ];

  slide.addChart(pres.charts.BAR, chartDataBar, {
    x: 0.5, y: 1.75, w: 4.6, h: 2.8,
    barDir: "col",
    barGrouping: "clustered",
    chartColors: [theme.light, theme.accent],
    showLegend: true,
    legendPos: "b",
    legendFontSize: 9,
    legendFontFace: "Microsoft YaHei",
    catAxisLabelFontSize: 9,
    catAxisLabelFontFace: "Microsoft YaHei",
    valAxisLabelFontSize: 8,
    showValue: true,
    dataLabelFontSize: 7,
    dataLabelColor: theme.primary,
    valAxisTitle: "Mpps",
    valAxisTitleFontSize: 9,
    showValAxisTitle: true,
    valGridLine: { style: "solid", color: "E2E8F0", size: 0.5 }
  });

  slide.addText("数据包大小 (B)", {
    x: 0.5, y: 4.4, w: 4.6, h: 0.2,
    fontSize: 8, fontFace: "Microsoft YaHei",
    color: theme.secondary, align: "center"
  });

  // 右侧：折线图 (线速占比)
  slide.addShape(pres.shapes.RECTANGLE, {
    x: 5.3, y: 1.3, w: 4.1, h: 3.5,
    fill: { color: "FFFFFF" }, line: { color: theme.light, width: 1 }
  });
  slide.addText("线速占比 (%)", {
    x: 5.5, y: 1.4, w: 4, h: 0.3,
    fontSize: 12, fontFace: "Microsoft YaHei",
    color: theme.primary, bold: true
  });

  const chartDataLine = [
    {
      name: "线速占比",
      labels: labels,
      values: percent
    }
  ];

  slide.addChart(pres.charts.LINE, chartDataLine, {
    x: 5.4, y: 1.75, w: 3.9, h: 2.8,
    chartColors: [theme.accent2],
    lineDataSymbol: "circle",
    lineDataSymbolSize: 8,
    lineSize: 3,
    showLegend: false,
    catAxisLabelFontSize: 9,
    catAxisLabelFontFace: "Microsoft YaHei",
    valAxisLabelFontSize: 8,
    valAxisMaxVal: 100,
    valAxisMinVal: 0,
    showValue: true,
    dataLabelFontSize: 8,
    dataLabelColor: theme.primary,
    dataLabelFormatCode: "0.0\"%\"",
    valGridLine: { style: "solid", color: "E2E8F0", size: 0.5 }
  });

  slide.addText("数据包大小 (B)", {
    x: 5.4, y: 4.4, w: 3.9, h: 0.2,
    fontSize: 8, fontFace: "Microsoft YaHei",
    color: theme.secondary, align: "center"
  });

  // 关键结论
  slide.addShape(pres.shapes.ROUNDED_RECTANGLE, {
    x: 0.4, y: 4.85, w: 4.8, h: 0.55,
    fill: { color: theme.primary }, rectRadius: 0.04
  });
  slide.addText([
    { text: "≥512B 大包：", options: { color: theme.accent2, bold: true, fontSize: 10 } },
    { text: "带宽 > 1Gbps，达到线速\n", options: { color: "FFFFFF", fontSize: 9 } },
    { text: "64B/128B 小包：", options: { color: theme.accent2, bold: true, fontSize: 10 } },
    { text: "受 vSwitch + VMXNET3 虚拟化开销限制", options: { color: "FFFFFF", fontSize: 9 } }
  ], {
    x: 0.5, y: 4.85, w: 4.6, h: 0.55,
    fontFace: "Microsoft YaHei", align: "left", valign: "middle"
  });

  // 关键数据
  slide.addShape(pres.shapes.ROUNDED_RECTANGLE, {
    x: 5.3, y: 4.85, w: 4.1, h: 0.55,
    fill: { color: "FFF7ED" }, line: { color: theme.accent2, width: 1 }, rectRadius: 0.04
  });
  slide.addText([
    { text: "0 丢包  ·  ", options: { color: theme.accent2, bold: true, fontSize: 11 } },
    { text: "0.449 Mpps  ·  ", options: { color: theme.primary, bold: true, fontSize: 11 } },
    { text: "230 Mbps  ·  ", options: { color: theme.primary, bold: true, fontSize: 11 } },
    { text: "3.18 Gbps (1514B)", options: { color: theme.accent2, bold: true, fontSize: 11 } }
  ], {
    x: 5.3, y: 4.85, w: 4.1, h: 0.55,
    fontFace: "Microsoft YaHei", align: "center", valign: "middle"
  });

  slide.addText("15", {
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
  pres.writeFile({ fileName: "slide-15-preview.pptx" });
}

module.exports = { createSlide, slideConfig };
