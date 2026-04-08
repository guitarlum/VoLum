const puppeteer = require('puppeteer');
const path = require('path');

(async () => {
  const htmlPath = path.resolve(__dirname, 'volum-mockup.html');
  const outPath = path.resolve(__dirname, 'screenshot.png');

  const browser = await puppeteer.launch({ headless: true });
  const page = await browser.newPage();
  await page.setViewport({ width: 960, height: 680, deviceScaleFactor: 2 });
  await page.goto('file:///' + htmlPath.replace(/\\/g, '/'), { waitUntil: 'networkidle0' });

  // Optional: click an amp to show detail view
  const showDetail = process.argv.includes('--detail');
  if (showDetail) {
    await page.evaluate(() => { selectAmp(0); });
    await new Promise(r => setTimeout(r, 500));
  }

  await page.screenshot({ path: outPath, fullPage: false });
  console.log('Screenshot saved to ' + outPath);
  await browser.close();
})();
