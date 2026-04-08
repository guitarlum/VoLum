const puppeteer = require('puppeteer');
const path = require('path');

(async () => {
  const htmlPath = path.resolve(__dirname, 'volum-variant-F.html');
  const browser = await puppeteer.launch({ headless: true });
  const page = await browser.newPage();
  await page.setViewport({ width: 960, height: 680, deviceScaleFactor: 2 });
  await page.goto('file:///' + htmlPath.replace(/\\/g, '/'), { waitUntil: 'networkidle0' });

  const ampIdx = parseInt(process.argv[2] || '0');
  if (ampIdx > 0) {
    await page.evaluate((idx) => {
      document.querySelectorAll('.amp-item')[idx].click();
    }, ampIdx);
    await new Promise(r => setTimeout(r, 300));
  }

  const outPath = path.resolve(__dirname, 'variant-F.png');
  await page.screenshot({ path: outPath, fullPage: false });
  console.log('Saved ' + outPath);
  await browser.close();
})();
