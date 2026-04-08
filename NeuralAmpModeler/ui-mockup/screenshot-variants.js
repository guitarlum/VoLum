const puppeteer = require('puppeteer');
const path = require('path');

(async () => {
  const htmlPath = path.resolve(__dirname, 'volum-variants.html');
  const browser = await puppeteer.launch({ headless: true });
  const page = await browser.newPage();
  await page.setViewport({ width: 960, height: 3200, deviceScaleFactor: 2 });
  await page.goto('file:///' + htmlPath.replace(/\\/g, '/'), { waitUntil: 'networkidle0' });

  const variants = ['vA', 'vB', 'vC', 'vD'];
  for (const id of variants) {
    const el = await page.$('#' + id);
    const outPath = path.resolve(__dirname, `variant-${id.slice(1)}.png`);
    await el.screenshot({ path: outPath });
    console.log('Saved ' + outPath);
  }

  await browser.close();
})();
