import assert from 'node:assert/strict';
import { createRequire } from 'node:module';
import { writeFile, readFile } from 'node:fs/promises';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';

const require = createRequire(import.meta.url);
const { chromium } = require('C:/Users/bagau/.cache/codex-runtimes/codex-primary-runtime/dependencies/node/node_modules/playwright');
const directory = dirname(fileURLToPath(import.meta.url));
const base = 'http://127.0.0.1:4176/';
const designs = ['tide', 'tide-relief', 'tide-glass', 'tide-signal', 'tide-console', 'tide-satin'];
const result = {
  startedAt: new Date().toISOString(),
  scope: 'Independent operator workflow checks in an isolated browser context; demo fixtures only.',
  checks: [], pageErrors: []
};
const fixesOnly = process.argv.includes('--review-fixes');
if (fixesOnly) {
  const previous = JSON.parse(await readFile(join(directory, 'operator-results.json'), 'utf8'));
  result.previousRunAt = previous.startedAt;
  result.checks = previous.checks.filter(check => !check.name.startsWith('Review fix:'));
  result.pageErrors = previous.pageErrors;
}
const browser = await chromium.launch({ headless: true, channel: 'msedge' });
const context = await browser.newContext({ viewport: { width: 1600, height: 1100 }, reducedMotion: 'reduce' });
const page = await context.newPage();
page.setDefaultTimeout(7000);
page.on('pageerror', error => result.pageErrors.push(String(error)));
const state = () => page.evaluate(() => window.MB.getState());
const plan = () => page.evaluate(() => window.MB.getPlan());
const reset = async (design = 'tide') => {
  await page.goto(`${base}${design}.html`);
  await page.waitForFunction(() => !!window.MB && !!window.MBSchedule);
  assert.equal(await page.locator('body').getAttribute('data-variant'), design);
  await page.evaluate(() => window.MB.reset());
};
const field = async (id, value) => {
  await page.locator(`#${id}`).fill(value);
  await page.locator(`#${id}`).dispatchEvent('change');
};
const nav = name => page.locator(`[data-page="${name}"]`).click();
const check = async (name, run) => {
  if (fixesOnly && !name.startsWith('Review fix:')) return;
  const started = Date.now();
  try {
    await run();
    result.checks.push({ name, status: 'passed', durationMs: Date.now() - started });
    console.log(`PASS ${name}`);
  } catch (error) {
    result.checks.push({ name, status: 'failed', durationMs: Date.now() - started, error: String(error) });
    console.log(`FAIL ${name}: ${String(error).slice(0, 500)}`);
  }
};
const saveChannel = async () => {
  await page.locator('#channel-form button[type="submit"]').click();
  await page.locator('dialog[open]').waitFor({ state: 'detached' });
};

try {
  await check('All six Tide variants: 10:24 resolves morning, next 12:00, precise advert 10:30', async () => {
    for (const design of designs) {
      await reset(design);
      const p = await plan();
      assert.equal(p.date, '2026-10-03', design);
      assert.equal(p.minute, 624, design);
      assert.equal(p.dayIndex, 5, design);
      assert.deepEqual(p.active.map(item => item.channel.name), ['Мягкое утро'], design);
      assert.equal(p.nextChannel.time, '12:00', design);
      assert.equal(p.nextChannel.channel.name, 'Дневной ритм', design);
      assert.equal(p.nextAd.time, '10:30', design);
      assert.equal(p.nextAd.file.title, 'Осенняя коллекция', design);
      assert.match(await page.locator('.ops-current').innerText(), /По расписанию в 10:24[\s\S]*Мягкое утро/i);
      assert.match(await page.locator('.ops-next').innerText(), /12:00[\s\S]*Дневной ритм/);
      assert.match(await page.locator('.ops-ad').innerText(), /10:30[\s\S]*Осенняя коллекция/);
    }
  });
  await check('Clearing date or time restores the active plan context', async () => {
    await reset(); await field('ops-date', ''); await field('ops-time', '');
    assert.equal(await page.locator('#ops-date').inputValue(), '2026-10-03');
    assert.equal(await page.locator('#ops-time').inputValue(), '10:24');
    await field('ops-date', '2026-11-01');
    assert.match(await page.locator('.ops-next').innerText(), /В выбранный день/);
    assert.doesNotMatch(await page.locator('.ops-next').innerText(), /Сегодня/);
  });
  await check('13:00 recalculates the plan independently of selected morning channel', async () => {
    await reset();
    await field('ops-time', '13:00');
    const p = await plan(), s = await state();
    assert.equal(s.opsTime, '13:00');
    assert.equal(s.active.music, 'm1');
    assert.deepEqual(p.active.map(item => item.channel.name), ['Дневной ритм']);
    assert.equal(p.nextChannel.time, '17:00');
    assert.match(await page.locator('.ops-current').innerText(), /Дневной ритм/);
    assert.match(await page.locator('.ops-selection').innerText(), /Мягкое утро/);
    assert.match(await page.locator('.inspector-rule').innerText(), /Завершён/);
  });
  await check('Date input and weekday selection agree across a week and month boundary', async () => {
    await reset();
    await field('ops-date', '2026-11-01');
    assert.equal((await plan()).dayIndex, 6);
    assert.equal(await page.locator('.day-button.active').getAttribute('data-day'), '6');
    assert.equal(await page.locator('.day-button.active').getAttribute('title'), '2026-11-01');
    assert.match(await page.locator('.inspector-rule').innerText(), /01\.11\.2026/);
    await page.locator('[data-day="0"]').click();
    assert.equal((await state()).opsDate, '2026-10-26');
    assert.equal((await plan()).dayIndex, 0);
    assert.equal(await page.locator('#ops-date').inputValue(), '2026-10-26');
    assert.equal(await page.locator('.day-button.active').getAttribute('title'), '2026-10-26');
  });
  await check('Editing weekday, month, and day-number conditions updates the why inspector', async () => {
    await reset();
    await page.locator('[data-action="edit-channel"]:visible').first().click();
    await page.locator('#channel-form [name="weekday"][value="5"]').uncheck();
    await saveChannel();
    assert.deepEqual((await plan()).active, []);
    assert.match(await page.locator('.inspector-rule').innerText(), /День недели исключён/);
    await page.locator('[data-action="edit-channel"]:visible').first().click();
    await page.locator('#channel-form [name="weekday"][value="5"]').check();
    await page.locator('#channel-form [name="month"][value="9"]').uncheck();
    await saveChannel();
    assert.deepEqual((await plan()).active, []);
    assert.match(await page.locator('.inspector-rule').innerText(), /Месяц исключён/);
    await page.locator('[data-action="edit-channel"]:visible').first().click();
    await page.locator('#channel-form [name="month"][value="9"]').check();
    await page.locator('#channel-form [name="dates"]').fill('4–31');
    await saveChannel();
    assert.deepEqual((await plan()).active, []);
    assert.match(await page.locator('.inspector-rule').innerText(), /День месяца исключён/);
    await field('ops-date', '2026-10-04');
    assert.deepEqual((await plan()).active.map(item => item.channel.name), ['Мягкое утро']);
    assert.match(await page.locator('.inspector-rule').innerText(), /календарные условия совпали/);
  });
  await check('Selecting a different channel only changes the editor and library context', async () => {
    await reset();
    const before = await plan();
    await page.locator('.channel-card[data-channel="m3"]').click();
    const after = await plan();
    assert.equal((await state()).active.music, 'm3');
    assert.deepEqual(after.active, before.active);
    assert.deepEqual(after.nextChannel, before.nextChannel);
    assert.match(await page.locator('.ops-selection').innerText(), /Ближе к вечеру/);
    assert.match(await page.locator('.ops-current').innerText(), /Мягкое утро/);
    assert.match(await page.locator('.inspector-rule').innerText(), /Позже/);
  });
  await check('22:00 respects exclusive interval end and shows tomorrow as next date', async () => {
    await reset();
    await field('ops-time', '22:00');
    const p = await plan();
    assert.deepEqual(p.active, []);
    assert.equal(p.nextChannel.time, '08:00');
    assert.equal(p.nextChannel.date, '2026-10-04');
    assert.equal(p.nextAd.time, '09:00');
    assert.equal(p.nextAd.date, '2026-10-04');
    assert.match(await page.locator('.ops-current').innerText(), /Нет активного канала/);
    assert.match(await page.locator('.ops-next').innerText(), /2026-10-04/);
  });
  await check('Music, video, advertising and service pages keep correct independent context', async () => {
    await reset();
    await field('ops-time', '13:00');
    await nav('video');
    assert.equal((await state()).page, 'video');
    assert.equal((await state()).opsTime, '13:00');
    assert.deepEqual((await plan()).active.map(item => item.channel.name), ['Природа крупным планом']);
    assert.equal((await plan()).nextChannel.time, '18:00');
    assert.equal(await page.locator('.channel-schedule tbody tr').count(), 2);
    await nav('ads');
    assert.equal((await state()).page, 'ads');
    assert.equal(await page.locator('.ads-table tbody tr').count(), 2);
    assert.equal((await plan()).nextAd.time, '13:30');
    for (const destination of ['report', 'settings', 'about']) {
      await nav(destination);
      assert.equal((await state()).page, destination);
      assert.equal(await page.locator('.page-heading').count(), 1);
      assert.equal(await page.locator('.ops-overview').count(), 0);
    }
    await nav('music');
    assert.equal((await state()).opsTime, '13:00');
    assert.deepEqual((await plan()).active.map(item => item.channel.name), ['Дневной ритм']);
  });
  await check('Review fix: station header follows network and offline modes', async () => {
    await reset();
    await page.locator('.station-button').click();
    await page.locator('[data-station="network"]').click();
    assert.equal((await state()).data.station, 'network');
    assert.match(await page.locator('.header-context').innerText(), /Сетевая медиастанция/);
    assert.doesNotMatch(await page.locator('.header-context').innerText(), /Локальная/);
    await page.locator('.station-button').click();
    await page.locator('[data-station="offline"]').click();
    assert.equal((await state()).data.station, 'offline');
    assert.match(await page.locator('.header-context').innerText(), /Подготовка без плеера/);
    assert(await page.locator('[data-action="play"]').isDisabled());
  });
  await check('Review fix: advertising details show selected advert rules and exact current minute', async () => {
    await reset();
    await nav('ads');
    await page.locator('.inspector-rule [data-action="ops-details"]').click();
    assert.equal(await page.locator('#dialog-title').innerText(), 'Условия рекламного выхода');
    const details = await page.locator('dialog[open]').innerText();
    assert.match(details, /Осенняя коллекция\.mp3/);
    assert.match(details, /9–20 ч/);
    assert.match(details, /00, 30 мин/);
    assert.match(details, /01\.10\.2026–31\.10\.2026/);
    assert.doesNotMatch(details, /Мягкое утро|Дневной ритм/);
    await page.keyboard.press('Escape');
    await page.locator('dialog[open]').waitFor({ state: 'detached' });
    await field('ops-time', '10:30');
    assert.equal((await plan()).exactAdsNow[0].file.title, 'Осенняя коллекция');
    assert.match(await page.locator('.ops-ad').innerText(), /Выход рекламы в 10:30[\s\S]*Осенняя коллекция/i);
    assert.match(await page.locator('.ops-ad').innerText(), /далее 11:00/);
  });
} finally {
  result.completedAt = new Date().toISOString();
  result.summary = { passed: result.checks.filter(check => check.status === 'passed').length, failed: result.checks.filter(check => check.status === 'failed').length, pageErrors: result.pageErrors.length };
  await writeFile(join(directory, 'operator-results.json'), JSON.stringify(result, null, 2));
  await browser.close();
}
if (result.summary.failed || result.summary.pageErrors) process.exitCode = 1;
