'use strict';
(() => {
  // Design-preview calculations, not MediaBoxPlayer telemetry or its scheduler.
  // ChannelData only stores these fields. In this preview, a crossing-midnight
  // window belongs to its START date, so its tail uses yesterday's filters.
  // Equal start/end is ambiguous without a day offset: do not invent 24h playback.
  const DAY = 86400000;
  const LOOKAHEAD = 370;
  const weekdays = ['Пн', 'Вт', 'Ср', 'Чт', 'Пт', 'Сб', 'Вс'];
  const dateString = date => date.toISOString().slice(0, 10);
  const addDays = (date, count) => new Date(date.getTime() + count * DAY);
  const dayIndex = date => (date.getUTCDay() + 6) % 7;
  const formatMinute = value => `${String(Math.floor(value / 60)).padStart(2, '0')}:${String(value % 60).padStart(2, '0')}`;
  function parseDate(value) {
    if (!/^\d{4}-\d{2}-\d{2}$/.test(value || '')) return null;
    const date = new Date(value + 'T00:00:00Z');
    return Number.isFinite(date.getTime()) && dateString(date) === value ? date : null;
  }
  function parseTime(value) {
    const match = /^(\d{2}):(\d{2})$/.exec(value || '');
    return match && +match[1] < 24 && +match[2] < 60 ? +match[1] * 60 + +match[2] : null;
  }
  function rangeValues(value, min, max, wrap = false) {
    if (String(value).trim() === '*') return Array.from({length: max - min + 1}, (_, index) => index + min);
    const values = new Set();
    for (const part of String(value ?? '').split(/[,;]/)) {
      const match = /^(\d+)(?:\s*[-–—]\s*(\d+))?$/.exec(part.trim());
      if (!match) return [];
      const from = +match[1], to = match[2] == null ? from : +match[2];
      if (from < min || from > max || to < min || to > max || (!wrap && to < from)) return [];
      if (to >= from) for (let number = from; number <= to; number++) values.add(number);
      else {
        for (let number = from; number <= max; number++) values.add(number);
        for (let number = min; number <= to; number++) values.add(number);
      }
    }
    return [...values].sort((a, b) => a - b);
  }
  function calendarReason(channel, date) {
    if (!(channel.days || []).includes(dayIndex(date))) return 'День недели исключён из расписания';
    if (!(channel.months || []).includes(date.getUTCMonth())) return 'Месяц исключён из расписания';
    if (!rangeValues(channel.dates, 1, 31).includes(date.getUTCDate())) return 'День месяца исключён из расписания';
    return '';
  }
  function channelWindows(channel, date) {
    const start = parseTime(channel.start), end = parseTime(channel.end);
    if (start === null || end === null || start === end) return [];
    const segments = [];
    if (start > end) {
      const previous = addDays(date, -1);
      if (!calendarReason(channel, previous) && end > 0) segments.push({
        start: 0, end,
        reason: `Продолжение окна ${channel.start}–${channel.end}, начатого ${dateString(previous)} (${weekdays[dayIndex(previous)]})`,
        startDate: dateString(previous)
      });
    }
    if (!calendarReason(channel, date)) segments.push({
      start, end: start < end ? end : 1440,
      reason: `${weekdays[dayIndex(date)]} · ${channel.start}–${channel.end}${start > end ? ' следующего дня' : ''} · календарные условия совпали`,
      startDate: dateString(date)
    });
    return segments;
  }
  function validAdDate(rule, date) {
    const from = parseDate(rule.start), until = parseDate(rule.end);
    return !!from && !!until && date >= from && date <= until && (rule.days || []).includes(dayIndex(date));
  }
  function scheduleIssues(lanes) {
    const issues = [];
    const segments = lanes.flatMap(lane => lane.segments.map(segment => ({...segment, channel: lane.channel})));
    const points = [...new Set(segments.flatMap(segment => [segment.start, segment.end]))].sort((a, b) => a - b);
    for (let index = 0; index < points.length - 1; index++) {
      const start = points[index], end = points[index + 1];
      const channels = [...new Set(segments.filter(segment => segment.start <= start && segment.end > start).map(segment => segment.channel))];
      if (channels.length > 1) issues.push({
        type: 'overlap', start, end, channels,
        message: `${formatMinute(start)}–${formatMinute(end)}: пересекаются ${channels.map(channel => '«' + channel.name + '»').join(', ')}. Приоритет не задан.`
      });
      // Only gaps BETWEEN scheduled programs are operational warnings.
      if (!channels.length) issues.push({type: 'gap', start, end, message: `${formatMinute(start)}–${formatMinute(end)}: между программами нет назначенного канала.`});
    }
    for (const lane of lanes) {
      if (lane.eligible && !(lane.channel.files || []).length) issues.push({type: 'empty', channel: lane.channel, message: `«${lane.channel.name}»: расписание есть, медиафайлы отсутствуют.`});
    }
    return issues;
  }
  function analyze({channels = [], rules = [], adFiles = [], date, time} = {}) {
    const selectedDate = parseDate(date), minute = parseTime(time);
    if (!selectedDate || minute === null) throw new TypeError('Ожидаются действительные дата YYYY-MM-DD и время HH:mm.');
    const lanes = channels.map(channel => {
      const segments = channelWindows(channel, selectedDate);
      const invalid = parseTime(channel.start) === null || parseTime(channel.end) === null;
      const equal = channel.start === channel.end;
      const current = segments.find(segment => segment.start <= minute && minute < segment.end);
      const upcoming = segments.find(segment => segment.start > minute);
      const last = segments.at(-1);
      const reason = invalid ? 'Некорректное время окна'
        : equal ? 'Начало совпадает с окончанием; длительность не определена'
        : current ? current.reason
        : upcoming ? `Окно ещё не началось · сегодня с ${formatMinute(upcoming.start)}`
        : last ? `Окно расписания на сегодня закончилось в ${formatMinute(last.end)}`
        : calendarReason(channel, selectedDate) || 'На выбранную дату нет окна';
      return {channel, segments, eligible: segments.length > 0, reason};
    });
    const active = lanes.flatMap(lane => lane.segments.filter(segment => segment.start <= minute && minute < segment.end).map(segment => ({channel: lane.channel, reason: segment.reason})));
    const issues = scheduleIssues(lanes);
    for (const lane of lanes) {
      if (!calendarReason(lane.channel, selectedDate) && (parseTime(lane.channel.start) === null || parseTime(lane.channel.end) === null || lane.channel.start === lane.channel.end)) {
        issues.push({type: 'empty', channel: lane.channel, message: `«${lane.channel.name}»: ${lane.reason.toLowerCase()}. Проверьте расписание.`});
      }
    }
    const resolvedRules = rules.map(rule => ({
      rule, file: adFiles.find(file => file.id === rule.fileId) || null,
      hours: rangeValues(rule.hours, 0, 23, true), minutes: rangeValues(rule.minutes, 0, 59)
    }));
    // This is a scheduled launch minute, not confirmation of audio playback or
    // an estimate of how long the advert occupies the player.
    const exactAdsNow = resolvedRules.filter(item => item.rule.mode === 'exact' && validAdDate(item.rule, selectedDate) && item.hours.includes(Math.floor(minute / 60)) && item.minutes.includes(minute % 60)).map(({rule, file}) => ({
      rule, file, reason: `Выход в ${formatMinute(minute)} по расписанию · точная минута запуска${file ? '' : ' · файл отсутствует'}`
    }));
    const frequencyRules = resolvedRules.filter(item => item.rule.mode === 'frequency' && validAdDate(item.rule, selectedDate) && item.hours.includes(Math.floor(minute / 60))).map(({rule, file}) => ({
      rule, file, reason: `${rule.frequency} выходов в час · точные минуты определяет плеер${file ? '' : ' · файл отсутствует'}`
    }));
    let nextChannel = null, nextAd = null;
    for (let offset = 0; offset < LOOKAHEAD && (!nextChannel || !nextAd); offset++) {
      const candidateDate = addDays(selectedDate, offset);
      if (!nextChannel) {
        for (const channel of channels) {
          const start = parseTime(channel.start), end = parseTime(channel.end);
          const minutesUntil = offset * 1440 + start - minute;
          if (start === null || end === null || start === end || minutesUntil <= 0 || calendarReason(channel, candidateDate)) continue;
          if (!nextChannel || minutesUntil < nextChannel.minutesUntil) nextChannel = {channel, time: formatMinute(start), date: dateString(candidateDate), minutesUntil};
        }
      }
      if (!nextAd) {
        for (const item of resolvedRules) {
          if (item.rule.mode !== 'exact' || !validAdDate(item.rule, candidateDate)) continue;
          for (const hour of item.hours) for (const adMinute of item.minutes) {
            const scheduledMinute = hour * 60 + adMinute, minutesUntil = offset * 1440 + scheduledMinute - minute;
            if (minutesUntil > 0 && (!nextAd || minutesUntil < nextAd.minutesUntil)) nextAd = {rule: item.rule, file: item.file, time: formatMinute(scheduledMinute), date: dateString(candidateDate), minutesUntil};
          }
        }
      }
    }
    const dateLabel = new Intl.DateTimeFormat('ru-RU', {day: 'numeric', month: 'long', timeZone: 'UTC'}).format(selectedDate);
    return {minute, dayIndex: dayIndex(selectedDate), date, dateLabel, active, nextChannel, nextAd, exactAdsNow, frequencyRules, lanes, issues,
      assumptions: ['Прогноз по расписанию; текущий файл и состояние плеера неизвестны.', 'Для окна через полночь календарные условия проверяются по дате начала.', 'Одинаковые начало и окончание не считаются полными сутками.'],
      horizonDays: LOOKAHEAD};
  }
  window.MBSchedule = Object.freeze({analyze, formatMinute});
})();
