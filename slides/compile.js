// slides/compile.js
const pptxgen = require("pptxgenjs");
const pres = new pptxgen();
pres.layout = 'LAYOUT_16x9';
pres.author = '杨毅';
pres.title = '基于DPDK的高性能数据包防火墙设计与实现 - 毕业答辩 v3.0';

const theme = {
  primary: "1E293B",
  secondary: "64748B",
  accent: "0D9488",
  accent2: "F97316",
  light: "F0FDF9",
  bg: "F5F7FA"
};

for (let i = 1; i <= 18; i++) {
  const num = String(i).padStart(2, '0');
  const slideModule = require(`./slide-${num}.js`);
  slideModule.createSlide(pres, theme);
}

pres.writeFile({ fileName: './output/graduation-defense3.0.pptx' })
  .then(() => console.log('PPTX generated: ./output/graduation-defense3.0.pptx'))
  .catch(err => console.error('Error:', err));
