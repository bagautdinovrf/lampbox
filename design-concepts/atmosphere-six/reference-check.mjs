import assert from 'node:assert/strict';
import { createRequire } from 'node:module';
import { access, readFile, writeFile, mkdir } from 'node:fs/promises';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';

const require = createRequire(import.meta.url);
const { chromium } = require('C:/Users/bagau/.cache/codex-runtimes/codex-primary-runtime/dependencies/node/node_modules/playwright');
const directory = dirname(fileURLToPath(import.meta.url));
const designs = ['tide', 'tide-relief', 'tide-glass', 'tide-signal', 'tide-console', 'tide-satin'];
for (const design of designs) await access(join(directory, design + '.html'));
const result = { startedAt: new Date().toISOString(), viewport: { width: 1440, height: 900 }, scope: 'Isolated browser contexts and demo data; no real media or application data changed.', checks: [], errors: [], screenshots: [], fonts: [] };
const sample = 'АБВГДЕЁЖЗИЙКЛМНОПРСТУФХЦЧШЩЪЫЬЭЮЯабвгдеёжзийклмнопрстуфхцчшщъыьэюя0123456789–—%';
async function verifyGlyphs(path) {
  const b = await readFile(path); let cmap;
  for (let i = 0; i < b.readUInt16BE(4); i++) { const p = 12 + i * 16; if (b.toString('ascii', p, p + 4) === 'cmap') cmap = b.readUInt32BE(p + 8); }
  assert(cmap !== undefined, `No cmap: ${path}`);
  const subtables = Array.from({ length: b.readUInt16BE(cmap + 2) }, (_, i) => cmap + b.readUInt32BE(cmap + 8 + i * 8));
  const has = cp => subtables.some(o => {
    const format = b.readUInt16BE(o);
    if (format === 12) { for (let i = 0; i < b.readUInt32BE(o + 12); i++) { const p = o + 16 + i * 12, start = b.readUInt32BE(p), end = b.readUInt32BE(p + 4); if (cp >= start && cp <= end) return b.readUInt32BE(p + 8) + cp - start !== 0; } }
    if (format === 4) { const n = b.readUInt16BE(o + 6) / 2, end = o + 14, start = end + n * 2 + 2, delta = start + n * 2, range = delta + n * 2; for (let i = 0; i < n; i++) { if (cp < b.readUInt16BE(start + i * 2) || cp > b.readUInt16BE(end + i * 2)) continue; const r = b.readUInt16BE(range + i * 2), d = b.readInt16BE(delta + i * 2); if (!r) return ((cp + d) & 65535) !== 0; const g = b.readUInt16BE(range + i * 2 + r + 2 * (cp - b.readUInt16BE(start + i * 2))); return g !== 0 && ((g + d) & 65535) !== 0; } }
    return false;
  });
  const missing = [...sample].filter(character => !has(character.codePointAt(0)));
  result.fonts.push({ path, missing }); assert.deepEqual(missing, []);
}
for (const path of ['C:/Windows/Fonts/segoeui.ttf', 'C:/Windows/Fonts/seguisb.ttf', 'C:/Windows/Fonts/segoeuib.ttf']) await verifyGlyphs(path);

const browser = await chromium.launch({ headless: true, channel: 'msedge' });
const visibleControl = async locator => {
  assert(await locator.isVisible());
  const box = await locator.boundingBox();
  assert(box && box.x >= -1 && box.y >= -1 && box.x + box.width <= 1441 && box.y + box.height <= 901, `Control outside viewport: ${JSON.stringify(box)}`);
  assert(await locator.evaluate(element => { const r = element.getBoundingClientRect(); const top = document.elementFromPoint(r.x + r.width / 2, r.y + r.height / 2); return top === element || element.contains(top); }), 'Control is obscured');
};
const check = async (design, name, run) => {
  const started = Date.now();
  try { await run(); result.checks.push({ design, name, status: 'passed', durationMs: Date.now() - started }); console.log(`PASS ${design}: ${name}`); }
  catch (error) { result.checks.push({ design, name, status: 'failed', durationMs: Date.now() - started, error: String(error) }); console.log(`FAIL ${design}: ${name}: ${String(error).slice(0, 450)}`); }
};

try {
  await mkdir(join(directory, 'previews'), { recursive: true });
  for (const design of designs) {
    const context = await browser.newContext({ viewport: result.viewport, reducedMotion: 'reduce' });
    const page = await context.newPage(); page.setDefaultTimeout(6500);
    page.on('pageerror', error => result.errors.push({ design, message: String(error) }));
    const reset = async () => { await page.goto(`http://127.0.0.1:4176/${design}.html`); await page.waitForFunction(() => !!window.MB); assert.equal(await page.locator('body').getAttribute('data-variant'), design); await page.evaluate(() => window.MB.reset()); };
    const state = () => page.evaluate(() => window.MB.getState());
    const nav = destination => page.locator(`[data-page="${destination}"]`).click();
    const action = name => page.locator(`[data-action="${name}"]`).first().click();
    const close = async () => { await page.keyboard.press('Escape'); await page.locator('dialog[open]').waitFor({ state: 'detached' }); };
    const readyFonts = async () => { const loaded = await page.evaluate(async sample => { await document.fonts.ready; for (const weight of [400, 600, 700]) await document.fonts.load(`${weight} 14px "Lamp Sans"`, sample); return [400, 600, 700].every(weight => document.fonts.check(`${weight} 14px "Lamp Sans"`, sample)); }, sample); assert(loaded); };
    const edit = async () => { await page.locator('.inspector-rule [data-action="edit-channel"]').click(); await page.locator('#channel-form').waitFor(); };
    const changeChannel = async () => {
      await page.locator('#channel-form [name="start"]').fill('08:30');
      await page.locator('#channel-form [name="end"]').fill('11:45');
      await page.locator('#channel-form [name="weekday"][value="0"]').uncheck();
      await page.locator('#channel-form [name="month"][value="0"]').uncheck();
      await page.locator('#channel-form [name="dates"]').fill('2–31');
      await page.locator('#channel-form [name="volume"]').focus();
      await page.keyboard.press('ArrowLeft');
    };
    await check(design, 'Main viewport, font load, all six navigation destinations', async () => {
      await reset(); await readyFonts();
      assert.equal(await page.locator('.palette-picker option').count(), 16);
      assert.equal(await page.evaluate(() => document.documentElement.scrollWidth), 1440);
      assert.equal(await page.evaluate(() => document.documentElement.scrollHeight), 900);
      await page.screenshot({ path: join(directory, 'previews', `${design}-check-main.png`), fullPage: false });
      result.screenshots.push(`previews/${design}-check-main.png`);
      for (const destination of ['music', 'video', 'ads', 'report', 'settings', 'about']) {
        await visibleControl(page.locator(`[data-page="${destination}"]`));
        await nav(destination);
        assert.equal((await state()).page, destination);
        assert.equal(await page.locator('.page-heading').count(), 1);
      }
    });
    await check(design, 'Channel dialog controls screenshot and cancel preserve all fields', async () => {
      await reset(); const before = (await state()).data.music[0]; await edit(); await readyFonts();
      await page.screenshot({ path: join(directory, 'previews', `${design}-controls.png`), fullPage: false });
      result.screenshots.push(`previews/${design}-controls.png`);
      await changeChannel();
      assert.equal(await page.locator('#volume-value').innerText(), '64%');
      await page.locator('#channel-form [data-action="close"]').click();
      await page.locator('dialog[open]').waitFor({ state: 'detached' });
      assert.deepEqual((await state()).data.music[0], before);
    });
    await check(design, 'Channel time, weekday, month, day numbers and volume save together', async () => {
      await reset(); await edit(); await changeChannel();
      await page.locator('#channel-form button[type="submit"]').click();
      await page.locator('dialog[open]').waitFor({ state: 'detached' });
      const channel = (await state()).data.music[0];
      assert.equal(channel.start, '08:30'); assert.equal(channel.end, '11:45');
      assert.deepEqual(channel.days, [1, 2, 3, 4, 5, 6]);
      assert.deepEqual(channel.months, [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11]);
      assert.equal(channel.dates, '2–31'); assert.equal(channel.volume, 64);
      await page.reload(); assert.deepEqual((await state()).data.music[0], channel);
      await edit(); assert.equal(await page.locator('#channel-form [name="start"]').inputValue(), '08:30');
      assert(!(await page.locator('#channel-form [name="weekday"][value="0"]').isChecked()));
      assert(!(await page.locator('#channel-form [name="month"][value="0"]').isChecked()));
      await close();
    });
    await check(design, 'Settings checkboxes, select all and clear all persist', async () => {
      await reset(); await nav('settings');
      await action('formats-all'); assert(Object.values((await state()).data.formats).every(Boolean));
      assert.equal(await page.locator('[data-format]:checked').count(), 9);
      await action('formats-none'); assert(Object.values((await state()).data.formats).every(value => !value));
      assert.equal(await page.locator('[data-format]:checked').count(), 0);
      await page.locator('[data-format="mp3"]').check();
      assert.equal((await state()).data.formats.mp3, true);
      await page.reload(); await nav('settings'); assert(await page.locator('[data-format="mp3"]').isChecked());
    });
    await check(design, 'Advertising frequency edit supports cancel and save', async () => {
      await reset(); await nav('ads'); const before = (await state()).data.rules.find(rule => rule.id === 'r2');
      await page.locator('[data-action="edit-rule"][data-id="r2"]').first().click();
      await page.locator('#rule-form [name="frequency"]').fill('4');
      await page.locator('#rule-form [data-action="close"]').click();
      await page.locator('dialog[open]').waitFor({ state: 'detached' });
      assert.deepEqual((await state()).data.rules.find(rule => rule.id === 'r2'), before);
      await page.locator('[data-action="edit-rule"][data-id="r2"]').first().click();
      await page.locator('#rule-form [name="frequency"]').fill('4');
      await page.locator('#rule-form button[type="submit"]').click();
      await page.locator('dialog[open]').waitFor({ state: 'detached' });
      assert.equal((await state()).data.rules.find(rule => rule.id === 'r2').frequency, 4);
      assert.match(await page.locator('.ads-table').innerText(), /4 \/ час/);
    });
    await check(design, 'Preview and scheduled play/stop remain reachable', async () => {
      await reset(); await page.locator('.inspector-actions [data-action="preview"]').click();
      assert.match(await page.locator('.transport-copy').innerText(), /Предпрослушивание · демо/);
      await page.locator('.inspector-actions [data-action="preview"]').click();
      assert.doesNotMatch(await page.locator('.transport-copy').innerText(), /Предпрослушивание/);
      await visibleControl(page.locator('[data-action="play"]')); await action('play'); assert.equal((await state()).playing, true);
      await visibleControl(page.locator('[data-action="stop"]')); await action('stop'); assert.equal((await state()).playing, false);
    });
    await check(design, 'Blank date and time restore previous valid inputs', async () => {
      await reset();
      for (const [id, expected] of [['ops-date', '2026-10-03'], ['ops-time', '10:24']]) {
        await page.locator(`#${id}`).fill(''); await page.locator(`#${id}`).dispatchEvent('change');
        assert.equal(await page.locator(`#${id}`).inputValue(), expected);
      }
      assert.deepEqual(await page.evaluate(() => MB.getPlan().active.map(item => item.channel.name)), ['Мягкое утро']);
    });
    await context.close();
  }
} finally {
  result.completedAt = new Date().toISOString();
  result.summary = { passed: result.checks.filter(check => check.status === 'passed').length, failed: result.checks.filter(check => check.status === 'failed').length, pageErrors: result.errors.length };
  await writeFile(join(directory, 'reference-results.json'), JSON.stringify(result, null, 2));
  await browser.close();
}
if (result.summary.failed || result.summary.pageErrors) process.exitCode = 1;
