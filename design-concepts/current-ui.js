(function () {
  'use strict';

  const allDays = [1, 2, 3, 4, 5, 6, 7];
  const allMonths = Array.from({length: 12}, (_, i) => i + 1);
  const dayNames = ['Пн', 'Вт', 'Ср', 'Чт', 'Пт', 'Сб', 'Вс'];
  const monthNames = ['Янв', 'Фев', 'Мар', 'Апр', 'Май', 'Июн', 'Июл', 'Авг', 'Сен', 'Окт', 'Ноя', 'Дек'];
  const formats = {audio: ['mp3', 'flac', 'ogg', 'wma', 'acc'], video: ['mp4', 'avi', 'mkv', 'wmv']};
  const esc = value => String(value ?? '').replace(/[&<>"']/g, c => ({'&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;'}[c]));
  const field = (label, name, value, attrs = '') => `<label class="field"><span class="field-label">${esc(label)}</span><input class="field-control" name="${esc(name)}" value="${esc(value)}" ${attrs}></label>`;
  const select = (label, name, value, options) => `<label class="field"><span class="field-label">${esc(label)}</span><select class="field-control" name="${esc(name)}">${options.map(o => `<option value="${esc(o.value)}" ${String(value) === String(o.value) ? 'selected' : ''}>${esc(o.label)}</option>`).join('')}</select></label>`;
  const checkboxes = (name, choices, selected, labels, extraClass = '') => `<div class="${extraClass || 'weekday-picker'}">${choices.map((n, i) => `<label class="check-line ${extraClass === 'month-picker' ? 'month-btn' : ''}"><input type="checkbox" name="${esc(name)}" value="${n}" ${selected.includes(n) ? 'checked' : ''}>${esc(labels[i])}</label>`).join('')}</div>`;
  const dataOf = form => form instanceof FormData ? form : new FormData(form);
  const raw = (data, name) => String(data.get(name) ?? '').trim();
  const number = (data, name) => {
    const text = raw(data, name), parsed = Number(text);
    return text === '' || !Number.isFinite(parsed) ? text : parsed;
  };
  const values = (data, name) => data.getAll(name).map(Number);
  const validSelection = (list, allowed) => list.length > 0 && list.every(n => Number.isInteger(n) && allowed.includes(n));
  const integerIn = (n, min, max) => Number.isInteger(n) && n >= min && n <= max;
  const defaultDays = row => Array.isArray(row?.days) ? row.days : allDays;
  const defaultMonths = row => Array.isArray(row?.months) ? row.months : allMonths;
  const defaultVolume = row => row?.volume ?? 65;

  function numberSet(value, min, max) {
    const text = String(value ?? '*').trim();
    if (text === '*') return Array.from({length: max - min + 1}, (_, i) => min + i);
    if (!text) return null;
    const result = new Set();
    for (const token of text.split(',')) {
      const match = token.trim().match(/^(\d+)(?:\s*-\s*(\d+))?$/);
      if (!match) return null;
      const start = Number(match[1]), end = Number(match[2] ?? match[1]);
      if (!integerIn(start, min, max) || !integerIn(end, min, max) || start > end) return null;
      for (let n = start; n <= end; n++) result.add(n);
    }
    return [...result].sort((a, b) => a - b);
  }
  function validDate(iso) {
    if (!/^\d{4}-\d{2}-\d{2}$/.test(iso)) return false;
    const date = new Date(iso + 'T12:00:00Z');
    return !isNaN(date) && date.toISOString().slice(0, 10) === iso;
  }
  function timeMinutes(time) {
    if (!/^([01]\d|2[0-3]):[0-5]\d$/.test(time)) return NaN;
    const [h, m] = time.split(':').map(Number);
    return h * 60 + m;
  }
  const intersect = (a, b) => a.some(n => b.includes(n));

  function templateEditor(rows, playlists) {
    return `<form id="template-form" novalidate><p class="panel-note">Настройте время и канал. В дополнительных условиях выберите дни, месяцы и громкость.</p><div class="template-editor">${rows.map((row, i) => `<section class="channel-criteria"><div class="panel-head"><h3>Интервал ${i + 1}</h3><button type="button" class="btn btn-ghost" data-action="template-delete" data-index="${i}">Удалить</button></div><div class="template-row">${field('Начало', 'start-' + i, row.start, 'type="time" required')}${field('Окончание', 'end-' + i, row.end, 'type="time" required')}${select('Плейлист', 'playlist-' + i, row.playlist, playlists.map(p => ({value: p.id, label: p.name})))}</div><details class="interval-details"><summary>Дни, месяцы и громкость · ${defaultDays(row).map(n=>dayNames[n-1]).join(', ')} · ${defaultVolume(row)}%</summary><div class="field-grid">${field('Числа месяца', 'dayNumbers-' + i, row.dayNumbers ?? '*', 'placeholder="* или 1,5,10-15" required')}${field('Громкость, %', 'volume-' + i, defaultVolume(row), 'type="number" min="0" max="100" required')}</div><div class="rule-options"><span class="field-label">Дни недели</span>${checkboxes('days-' + i, allDays, defaultDays(row), dayNames)}</div><div class="rule-options"><span class="field-label">Месяцы</span>${checkboxes('months-' + i, allMonths, defaultMonths(row), monthNames, 'month-picker')}</div></details></section>`).join('')}</div><div class="editor-actions"><button type="button" class="btn" data-action="template-add">Добавить интервал</button></div><p class="panel-note">«*» означает все числа месяца. Окончание 00:00 означает конец суток.</p></form>`;
  }

  function readTemplate(form, oldRows) {
    const data = dataOf(form);
    const value = oldRows.map((row, i) => ({...row, start: raw(data, 'start-' + i), end: raw(data, 'end-' + i), playlist: raw(data, 'playlist-' + i), days: values(data, 'days-' + i), dayNumbers: raw(data, 'dayNumbers-' + i), months: values(data, 'months-' + i), volume: number(data, 'volume-' + i)}));
    let error = '';
    for (let i = 0; i < value.length; i++) {
      const row = value[i], start = timeMinutes(row.start), end = row.end === '00:00' ? 1440 : timeMinutes(row.end);
      if (!Number.isFinite(start) || !Number.isFinite(end) || start >= end) error = `Интервал ${i + 1}: начало должно быть раньше окончания. Для конца суток укажите 00:00.`;
      else if (!row.playlist) error = `Интервал ${i + 1}: выберите плейлист.`;
      else if (!validSelection(row.days, allDays)) error = `Интервал ${i + 1}: выберите хотя бы один день недели.`;
      else if (!numberSet(row.dayNumbers, 1, 31)) error = `Интервал ${i + 1}: укажите «*», числа 1–31 или диапазоны, например 1,5,10-15.`;
      else if (!validSelection(row.months, allMonths)) error = `Интервал ${i + 1}: выберите хотя бы один месяц.`;
      else if (!integerIn(row.volume, 0, 100)) error = `Интервал ${i + 1}: громкость должна быть целым числом от 0 до 100.`;
      if (error) return {value, error};
    }
    for (let i = 0; i < value.length; i++) {
      for (let j = i + 1; j < value.length; j++) {
        const a = value[i], b = value[j];
        const timeOverlap = timeMinutes(a.start) < (b.end === '00:00' ? 1440 : timeMinutes(b.end)) && timeMinutes(b.start) < (a.end === '00:00' ? 1440 : timeMinutes(a.end));
        if (timeOverlap && intersect(a.days, b.days) && intersect(a.months, b.months) && intersect(numberSet(a.dayNumbers, 1, 31), numberSet(b.dayNumbers, 1, 31))) return {value, error: `Интервалы ${i + 1} и ${j + 1} пересекаются по времени и календарным условиям.`};
      }
    }
    return {value, error: ''};
  }

  function advertEditor(a, files) {
    const fileId = a.fileId ?? a.filename ?? '';
    const options = [{value: '', label: 'Выберите файл из хранилища'}, ...files.map(f => ({value: f.id, label: `${f.title} · ${f.mediaType === 'video' ? 'видео' : 'аудио'}`}))];
    if (fileId && !options.some(o => String(o.value) === String(fileId))) options.push({value: fileId, label: a.filename || fileId});
    return `<form id="advert-form" novalidate><div class="field-grid">${field('Название', 'name', a.name, 'maxlength="80" required')}${select('Рекламный файл', 'fileId', fileId, options)}${field('Часы запуска', 'hours', a.hours ?? '9-21', 'placeholder="* или 9-21" required')}${select('Режим запуска', 'scheduleMode', a.scheduleMode ?? 'frequency', [{value: 'frequency', label: 'Частота в час'}, {value: 'minutes', label: 'Точные минуты часа'}])}${field('Запусков в час', 'frequency', a.frequency ?? 1, 'type="number" min="1" max="5" required')}${field('Минуты часа', 'minuteNumbers', a.minuteNumbers ?? '0,15,30,45', 'placeholder="0,15,30,45"')}${field('С какой даты', 'start', a.start, 'type="date" required')}${field('По какую дату включительно', 'end', a.end, 'type="date" required')}${field('Громкость, %', 'volume', defaultVolume(a), 'type="number" min="0" max="100" required')}${field('Длительность демо, секунд', 'duration', a.duration ?? 20, 'type="number" min="1" max="3600" required')}</div><div class="rule-options"><span class="field-label">Дни недели</span>${checkboxes('days', allDays, defaultDays(a), dayNames)}</div><label class="check-line"><input type="checkbox" name="enabled" ${a.enabled !== false ? 'checked' : ''}>Вставка включена</label><p class="panel-note">Текущая реклама может быть аудио или видео. Частота задаёт количество запусков в час; точные минуты — позиции внутри каждого выбранного часа.</p><p class="panel-note">Название и длительность нужны для демонстрации. Прототип не воспроизводит и не копирует файлы.</p><input type="hidden" name="fileMeta" value="${esc(JSON.stringify(files))}"></form>`;
  }

  function readAdvert(form, oldAdvert) {
    const data = dataOf(form);
    let files = [];
    try { files = JSON.parse(raw(data, 'fileMeta')); } catch { /* Legacy forms may have no metadata. */ }
    if (!Array.isArray(files)) files = [];
    const fileId = raw(data, 'fileId'), file = files.find(f => String(f.id) === fileId);
    const value = {...oldAdvert, name: raw(data, 'name'), fileId, filename: file ? file.title : fileId === String(oldAdvert.fileId ?? oldAdvert.filename ?? '') ? oldAdvert.filename || fileId : '', mediaType: file?.mediaType || oldAdvert.mediaType || 'audio', hours: raw(data, 'hours'), scheduleMode: raw(data, 'scheduleMode'), frequency: number(data, 'frequency'), minuteNumbers: raw(data, 'minuteNumbers'), days: values(data, 'days'), start: raw(data, 'start'), end: raw(data, 'end'), volume: number(data, 'volume'), duration: number(data, 'duration'), enabled: data.has('enabled')};
    let error = '';
    if (!value.name) error = 'Введите название рекламной вставки.';
    else if (value.name.length > 80) error = 'Название должно быть не длиннее 80 символов.';
    else if (!value.fileId || !value.filename) error = 'Выберите рекламный файл.';
    else if (!numberSet(value.hours, 0, 23)) error = 'Укажите часы 0–23 или диапазон, например 9-21; «*» — круглосуточно.';
    else if (!['frequency', 'minutes'].includes(value.scheduleMode)) error = 'Выберите режим запуска.';
    else if (value.scheduleMode === 'frequency' && !integerIn(value.frequency, 1, 5)) error = 'Укажите от 1 до 5 запусков в час.';
    else if (value.scheduleMode === 'minutes' && !numberSet(value.minuteNumbers, 0, 59)) error = 'Укажите минуты 0–59, например 0,15,30,45.';
    else if (!validSelection(value.days, allDays)) error = 'Выберите хотя бы один день недели.';
    else if (!validDate(value.start) || !validDate(value.end) || value.start > value.end) error = 'Проверьте даты начала и окончания.';
    else if (!integerIn(value.volume, 0, 100)) error = 'Громкость должна быть целым числом от 0 до 100.';
    else if (!integerIn(value.duration, 1, 3600)) error = 'Длительность демо должна быть от 1 до 3600 секунд.';
    return {value, error};
  }

  function settingsEditor(settings) {
    const formatGroup = kind => `<section class="channel-criteria"><div class="panel-head"><h3>${kind === 'audio' ? 'Аудиоформаты' : 'Видеоформаты'}</h3><div class="list-actions"><button type="button" class="btn btn-ghost" data-action="formats-all" data-kind="${kind}">Выбрать все</button><button type="button" class="btn btn-ghost" data-action="formats-none" data-kind="${kind}">Снять выбор</button></div></div><div class="month-picker">${formats[kind].map(format => `<label class="check-line"><input type="checkbox" name="${kind}Formats" value="${format}" ${(settings[kind + 'Formats'] ?? [kind === 'audio' ? 'mp3' : 'mp4']).includes(format) ? 'checked' : ''}>${format.toUpperCase()}</label>`).join('')}</div></section>`;
    return `<form data-settings-form class="settings-form" novalidate><div class="field-grid">${field('Название станции', 'name', settings.name, 'id="station-name" maxlength="80" required')}${select('Часовой пояс станции', 'zone', settings.zone ?? 'Europe/Moscow', [{value: 'Europe/Moscow', label: 'Москва · UTC+3'}, {value: 'UTC', label: 'UTC'}, {value: 'Asia/Yekaterinburg', label: 'Екатеринбург · UTC+5'}])}</div><p class="panel-note">Название и часовой пояс — предложение для новой архитектуры.</p>${formatGroup('audio')}${formatGroup('video')}<p class="panel-note">Если снять весь выбор, используются MP3 / MP4 по умолчанию.</p><div class="channel-criteria"><h3>Управление плеером</h3><div class="editor-actions"><button type="button" class="btn" data-action="refresh-player">Обновить расписание</button><button type="button" class="btn" data-action="show-player">Показать плеер</button><button type="button" class="btn" data-action="hide-player">Скрыть плеер</button></div><p class="panel-note">В прототипе эти команды изменяют только состояние демонстрации.</p></div><div class="editor-actions"><button class="btn btn-primary" type="submit">Сохранить настройки</button><button type="button" class="btn btn-ghost" data-action="reset-demo">Сбросить демо</button></div></form>`;
  }

  function readSettings(form, settings) {
    const data = dataOf(form);
    const value = {...settings, name: raw(data, 'name'), zone: raw(data, 'zone'), audioFormats: data.getAll('audioFormats').map(String), videoFormats: data.getAll('videoFormats').map(String)};
    let error = '';
    if (!value.name || value.name.length > 80) error = 'Введите название станции длиной до 80 символов.';
    else if (!['Europe/Moscow', 'UTC', 'Asia/Yekaterinburg'].includes(value.zone)) error = 'Выберите часовой пояс станции.';
    else if (value.audioFormats.some(f => !formats.audio.includes(f)) || value.videoFormats.some(f => !formats.video.includes(f))) error = 'Проверьте выбранные форматы файлов.';
    return {value, error};
  }

  function channelMatches(row, iso) {
    if (!validDate(iso)) return false;
    const date = new Date(iso + 'T12:00:00Z'), days = numberSet(row.dayNumbers ?? '*', 1, 31);
    return !!days && defaultDays(row).includes(date.getUTCDay() || 7) && defaultMonths(row).includes(date.getUTCMonth() + 1) && days.includes(date.getUTCDate());
  }

  window.MediaBoxCurrentUI = {templateEditor, readTemplate, advertEditor, readAdvert, settingsEditor, readSettings, channelMatches};
})();
