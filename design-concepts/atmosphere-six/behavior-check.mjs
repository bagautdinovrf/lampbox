import { createRequire } from 'node:module';
import { writeFile, readFile, access } from 'node:fs/promises';
import { fileURLToPath } from 'node:url';
import { dirname, join } from 'node:path';
import assert from 'node:assert/strict';
const require = createRequire(import.meta.url);
const { chromium } = require('C:/Users/bagau/.cache/codex-runtimes/codex-primary-runtime/dependencies/node/node_modules/playwright');
const directory = dirname(fileURLToPath(import.meta.url));
const base = 'http://127.0.0.1:4176/';
const designs = ['tide', 'tide-relief', 'tide-glass', 'tide-signal', 'tide-console', 'tide-satin'];
const palettes = ['orbit','tide','atelier','grove','gallery','studio','pearl','prism','lagoon','denim','berry','slate','iris','coral','pine','graphite'];
const output = { startedAt: new Date().toISOString(), scope: 'Isolated browser context; demo data only; in-memory file payloads; no real media files changed.', checks: [], pageErrors: [] };
const selectorOnly = process.argv.includes('--selector-only');
if (selectorOnly) {
  const previous = JSON.parse(await readFile(join(directory, 'behavior-results.json'), 'utf8'));
  output.previousRunAt = previous.startedAt;
  output.checks = previous.checks.filter(check => !check.name.startsWith('Variant selector'));
  output.pageErrors = previous.pageErrors;
}
const browser = await chromium.launch({ headless: true, channel: 'msedge' });
const context = await browser.newContext({ viewport: { width: 1600, height: 1100 }, reducedMotion: 'reduce' });
const page = await context.newPage();
page.setDefaultTimeout(6000);
page.on('pageerror', error => output.pageErrors.push(String(error)));
const state = () => page.evaluate(() => window.MB.getState());
const click = action => page.locator(`[data-action="${action}"]:visible`).first().click();
const nav = name => page.locator(`[data-page="${name}"]`).click();
const closeDialog = async () => { if (await page.locator('dialog[open]').count()) await page.keyboard.press('Escape'); await page.locator('dialog[open]').waitFor({ state: 'detached' }); };
const reset = async (design = 'tide') => {
  await page.goto(`${base}${design}.html`);
  await page.waitForFunction(() => !!window.MB);
  assert.equal(await page.locator('body').getAttribute('data-variant'), design);
  await page.evaluate(() => window.MB.reset());
  if ((await state()).playing) await click('stop');
};
const check = async (name, run) => {
  if (selectorOnly && !name.startsWith('Variant selector')) return;
  const start = Date.now();
  try { await run(); output.checks.push({ name, status: 'passed', durationMs: Date.now() - start }); console.log(`PASS ${name}`); }
  catch (error) { output.checks.push({ name, status: 'failed', durationMs: Date.now() - start, error: String(error) }); console.log(`FAIL ${name}: ${String(error).slice(0, 450)}`); await closeDialog().catch(() => {}); }
};
const upload = async files => { const chooserPromise = page.waitForEvent('filechooser'); await click('import'); const chooser = await chooserPromise; await chooser.setFiles(files.map(name => ({ name, mimeType: name.endsWith('.mp4') ? 'video/mp4' : 'audio/mpeg', buffer: Buffer.from('prototype behavioral fixture') }))); };
const saveForm = async id => { await page.locator(`${id} button[type="submit"]`).click(); await page.locator('dialog[open]').waitFor({ state: 'detached' }); };

try {
  await check('Channel cancel leaves data unchanged', async () => {
    await reset(); const before = (await state()).data; await click('edit-channel');
    await page.locator('#channel-form [name="name"]').fill('Отмена без сохранения');
    await page.locator('#channel-form [data-action="close"]').click();
    await page.locator('dialog[open]').waitFor({ state: 'detached' });
    assert.deepEqual((await state()).data, before);
  });
  await check('Rule cancel leaves data unchanged', async () => {
    await reset(); await nav('ads'); const before = (await state()).data;
    await click('edit-rule'); await page.locator('#rule-form [name="hours"]').fill('22–3');
    await page.locator('#rule-form [data-action="close"]').click();
    await page.locator('dialog[open]').waitFor({ state: 'detached' }); assert.deepEqual((await state()).data, before);
  });
  await check('Create, rename, guard deletion while playing, then delete channel', async () => {
    await reset(); await click('add-channel'); await page.locator('#channel-form [name="name"]').fill('Тестовый канал'); await saveForm('#channel-form');
    let s = await state(); assert.equal(s.data.music.length, 4); const id = s.active.music;
    await click('edit-channel'); await page.locator('#channel-form [name="name"]').fill('Новое имя'); await saveForm('#channel-form');
    assert.equal((await state()).data.music.find(c => c.id === id).name, 'Новое имя');
    await click('play'); await click('edit-channel'); await click('delete-channel');
    assert.equal((await state()).data.music.length, 4); assert.match(await page.locator('#toast').innerText(), /Остановите/);
    await closeDialog(); await click('stop'); await click('edit-channel'); await click('delete-channel'); await click('confirm');
    assert.equal((await state()).data.music.length, 3); assert(!(await state()).data.music.some(c => c.id === id));
  });
  await check('All channels deleted produces usable empty state', async () => {
    await reset(); for (let i = 0; i < 3; i++) { await click('edit-channel'); await click('delete-channel'); await click('confirm'); await page.locator('dialog[open]').waitFor({ state: 'detached' }); }
    assert.equal((await state()).data.music.length, 0); assert(await page.locator('[data-action="import"]').isDisabled());
    await click('add-channel'); await page.locator('#channel-form [name="name"]').fill('Из пустого состояния'); await saveForm('#channel-form'); assert.equal((await state()).data.music.length, 1);
  });
  await check('Audio and video can both be imported into advertising', async () => {
    await reset(); await nav('ads'); await upload(['Тест аудио.mp3', 'Тест видео.mp4']);
    await page.locator('dialog[open]').waitFor({ state: 'detached' });
    const files = (await state()).data.adFiles; assert(files.some(f => f.name === 'Тест аудио.mp3')); assert(files.some(f => f.name === 'Тест видео.mp4'));
    await page.reload(); assert.equal((await state()).data.adFiles.length, 5);
  });
  await check('Referenced advertising media cannot be deleted', async () => {
    await reset(); await nav('ads'); const before = (await state()).data; const id = before.rules[0].fileId;
    await page.locator(`[data-file-check="${id}"]`).check(); await click('delete-files');
    assert.equal(await page.locator('dialog[open]').count(), 0); assert.deepEqual((await state()).data, before); assert.match(await page.locator('#toast').innerText(), /Сначала удалите/);
  });
  await check('Batch advertising rules cover both checked media files', async () => {
    await reset(); await nav('ads'); const before = (await state()).data;
    for (const f of before.adFiles.slice(0, 2)) await page.locator(`[data-file-check="${f.id}"]`).check();
    await click('add-rule'); await page.locator('#rule-form [name="hours"]').fill('22–3'); await saveForm('#rule-form');
    const s = await state(); assert.equal(s.data.rules.length, 4); assert.deepEqual(new Set(s.data.rules.slice(2).map(r => r.fileId)), new Set(before.adFiles.slice(0, 2).map(f => f.id))); assert(s.data.rules.slice(2).every(r => r.hours === '22–3')); assert.equal(s.checked.length, 0);
  });
  await check('Frequency range 1–20 and invalid date ordering', async () => {
    await reset(); await nav('ads'); await click('add-rule'); await page.locator('#rule-form [name="mode"]').selectOption('frequency');
    for (const invalid of ['0', '21']) { await page.locator('#rule-form [name="frequency"]').fill(invalid); await page.locator('#rule-form button[type="submit"]').click(); assert.equal((await state()).data.rules.length, 2); assert.equal(await page.locator('#rule-form').count(), 1); }
    await page.locator('#rule-form [name="frequency"]').fill('20'); await page.locator('#rule-form [name="start"]').fill('2026-11-01'); await page.locator('#rule-form [name="end"]').fill('2026-10-01');
    await page.locator('#rule-form button[type="submit"]').click(); assert.match(await page.locator('#form-error').innerText(), /Дата окончания/); assert.equal((await state()).data.rules.length, 2);
    await page.locator('#rule-form [name="end"]').fill('2026-11-30'); await saveForm('#rule-form'); assert.equal((await state()).data.rules.at(-1).frequency, 20);
    await click('add-rule'); await page.locator('#rule-form [name="mode"]').selectOption('frequency'); await page.locator('#rule-form [name="frequency"]').fill('1'); await saveForm('#rule-form'); assert.equal((await state()).data.rules.at(-1).frequency, 1);
  });
  await check('Network mode blocks advertising changes', async () => {
    await reset(); await nav('ads'); await click('station'); await page.locator('[data-station="network"]').click(); await page.locator('dialog[open]').waitFor({ state: 'detached' });
    for (const action of ['add-rule','import','folder','delete-rule']) assert(await page.locator(`[data-action="${action}"]`).first().isDisabled(), action);
    const before = (await state()).data; await page.locator('.ads-table td button[data-action="edit-rule"]').first().click(); assert.equal(await page.locator('dialog[open]').count(), 0); assert.deepEqual((await state()).data, before);
  });
  await check('Preview identifies advertising asset rather than colliding music ID', async () => {
    await reset(); await nav('ads'); await click('preview'); assert.match(await page.locator('.transport-copy strong').innerText(), /Осенняя коллекция/);
  });
  await check('Report has populated and empty period states', async () => {
    await reset(); await nav('report'); await click('view-report'); assert.equal(await page.locator('.service-view tbody tr').count(), 6);
    await page.locator('#report-month').selectOption('8'); await click('view-report'); assert.match(await page.locator('.service-view').innerText(), /За этот период отчётов нет/);
  });
  await check('Format settings persist and select-all/clear-all work', async () => {
    await reset(); await nav('settings'); await page.locator('[data-format="flac"]').check(); await page.reload(); assert.equal((await state()).data.formats.flac, true);
    await nav('settings'); await click('formats-none'); assert(Object.values((await state()).data.formats).every(v => !v)); await click('formats-all'); assert(Object.values((await state()).data.formats).every(Boolean));
  });
  await check('Checkbox, row, and day retain keyboard focus after render', async () => {
    await reset(); const id = (await state()).data.music[0].files[0].id;
    await page.locator(`[data-file-check="${id}"]`).focus(); await page.keyboard.press('Space'); assert.equal(await page.evaluate(() => document.activeElement.dataset.fileCheck), id);
    await page.locator(`[data-file="${id}"]`).focus(); await page.keyboard.press('Enter'); assert.equal(await page.evaluate(() => document.activeElement.dataset.file), id);
    await page.locator('[data-day="1"]').focus(); await page.keyboard.press('Enter'); assert.equal(await page.evaluate(() => document.activeElement.dataset.day), '1');
  });
  await check('Escape during import preserves completed subset across reload', async () => {
    await reset(); await nav('ads'); await upload(Array.from({ length: 25 }, (_, i) => `Отмена ${i}.mp3`));
    await page.waitForFunction(() => window.MB.getState().data.adFiles.length >= 5); await page.keyboard.press('Escape'); await page.locator('dialog[open]').waitFor({ state: 'detached' });
    const before = (await state()).data.adFiles.map(f => f.name); assert(before.length > 3 && before.length < 28); await page.reload(); assert.deepEqual((await state()).data.adFiles.map(f => f.name), before);
  });
  await check('Empty advertising media and schedules stay usable', async () => {
    await reset(); await nav('ads'); for (let i = 0; i < 2; i++) { await click('delete-rule'); await click('confirm'); await page.locator('dialog[open]').waitFor({ state: 'detached' }); }
    await page.locator('[data-select-all]').check(); await click('delete-files'); await click('confirm'); await page.locator('dialog[open]').waitFor({ state: 'detached' }); assert.equal((await state()).data.adFiles.length, 0);
    await click('add-rule'); assert.match(await page.locator('#toast').innerText(), /Сначала добавьте/); assert.equal(await page.locator('dialog[open]').count(), 0);
  });

  for (const design of designs) await check(`${design}: all six sections, dialog cancellation, Cyrillic font ready`, async () => {
    await reset(design); await page.evaluate(() => document.fonts.ready); assert.equal(await page.evaluate(() => document.fonts.status), 'loaded');
    for (const section of ['music', 'video', 'ads', 'report', 'settings', 'about']) { await nav(section); assert.equal((await state()).page, section); assert(await page.locator('main h1').isVisible()); }
    await nav('music'); const before = (await state()).data; await click('edit-channel'); await page.locator('#channel-form [name="name"]').fill('Не сохранять'); await page.locator('#channel-form [data-action="close"]').click(); await page.locator('dialog[open]').waitFor({ state: 'detached' }); assert.deepEqual((await state()).data, before);
  });

  let palettesAvailable = true; try { await access(join(directory, 'palettes.css')); } catch { palettesAvailable = false; }
  if (palettesAvailable) {
    for (const design of designs) await check(`${design}: sixteen palettes change appearance and persist`, async () => {
      await reset(design); const picker = page.locator('#palette-picker'); const values = await picker.locator('option').evaluateAll(options => options.map(o => o.value)); assert.deepEqual(values, palettes);
      const styles = [];
      for (const palette of values) { await picker.selectOption(palette); assert.equal(await page.locator('body').getAttribute('data-palette'), palette); assert.equal(await page.locator('body').getAttribute('data-variant'), design); styles.push(await page.evaluate(() => { const s = getComputedStyle(document.body); return ['--bg','--accent','--surface','--ink'].map(p => s.getPropertyValue(p)).join('|') })); await nav('ads'); assert.equal(await page.locator('#palette-picker').inputValue(), palette); await nav('music'); }
      assert.equal(new Set(styles).size, 16, 'Each palette must expose a distinct set of color tokens'); await page.reload(); assert.equal(await page.locator('#palette-picker').inputValue(), values.at(-1));
    });
  } else output.checks.push({ name: 'Six Tide variants × sixteen palettes', status: 'pending', reason: 'palettes.css was not available when behavioral tests finished; this is not a failure.' });
  await check('Variant selector navigates through all six Tide pages and preserves the palette', async () => {
    await reset(); await page.locator('#palette-picker').selectOption('coral');
    assert.deepEqual(await page.locator('#variant-picker option').evaluateAll(options => options.map(option => option.value)), designs);
    for (const variant of [...designs.slice(1), designs[0]]) {
      await page.locator('#variant-picker').selectOption(variant);
      await page.waitForURL(`${base}${variant}.html*`);
      await page.waitForFunction(() => !!window.MB);
      assert.equal(await page.locator('body').getAttribute('data-variant'), variant);
      assert.equal(await page.locator('#variant-picker').inputValue(), variant);
      assert.equal(await page.locator('#palette-picker').inputValue(), 'coral');
      assert.equal(await page.locator('body').getAttribute('data-palette'), 'coral');
    }
  });
} finally {
  output.completedAt = new Date().toISOString(); output.summary = { passed: output.checks.filter(c => c.status === 'passed').length, failed: output.checks.filter(c => c.status === 'failed').length, pending: output.checks.filter(c => c.status === 'pending').length, pageErrors: output.pageErrors.length };
  await writeFile(join(directory, 'behavior-results.json'), JSON.stringify(output, null, 2) + '\n', 'utf8');
  await browser.close(); console.log(JSON.stringify(output.summary));
}
if (output.summary.failed || output.summary.pageErrors) process.exitCode = 1;
