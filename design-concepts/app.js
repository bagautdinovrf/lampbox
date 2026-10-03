(() => {
  'use strict';
  const variant = document.body.dataset.variant || 'air';
  const meta = {
    air: ['01', 'Air', 'Программа в вашем ритме', 'Расписание, ротация и очередь — рядом.'],
    studio: ['02', 'Studio', 'Дневной эфир', 'Два источника. Одна последовательность.'],
    canvas: ['03', 'Canvas', 'Календарь музыки', 'Выберите день и посмотрите, как он будет звучать.'],
    focus: ['04', 'Focus', 'Праздник в каждом втором треке', 'Настройте правило и проверьте результат в очереди.'],
    silver: ['05', 'Silver', 'Всё на своих местах', 'Правила, программа и эфир в одном рабочем пространстве.'],
    library: ['06', 'Library', 'У каждого дня своё звучание', 'Любимые коллекции и точное место в программе.'],
    timeline: ['07', 'Timeline', 'Ваш день в музыке', 'Обычная программа и праздничное подмешивание — отдельными слоями.']
  }[variant];
  const storageKey = 'lampbox-interactive-v3-' + variant;
  const clone = value => JSON.parse(JSON.stringify(value));
  const esc = value => String(value ?? '').replace(/[&<>"']/g, c => ({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
  const svg = (name, size = 18) => {
    const paths = {
      overview:'<path d="M4 9v6m4-10v14m4-16v18m4-16v14m4-10v6"/>',
      calendar:'<rect x="4" y="5" width="16" height="16" rx="3"/><path d="M8 3v4m8-4v4M4 11h16m-11 4h2m3 0h1"/>',
      rules:'<path d="M4 7h16M4 12h16M4 17h16"/><circle cx="8" cy="7" r="2" fill="currentColor"/><circle cx="16" cy="12" r="2" fill="currentColor"/><circle cx="10" cy="17" r="2" fill="currentColor"/>',
      library:'<rect x="4" y="4" width="6" height="6" rx="2"/><rect x="14" y="4" width="6" height="6" rx="2"/><rect x="4" y="14" width="6" height="6" rx="2"/><rect x="14" y="14" width="6" height="6" rx="2"/>',
      adverts:'<path d="M5 9h4l9-5v16l-9-5H5zM8 15l2 5"/>',
      settings:'<path d="M4 7h16M4 17h16M8 4v6m8 4v6"/>',
      play:'<path d="m9 5 11 7-11 7z" fill="currentColor" stroke="none"/>',
      pause:'<path d="M9 6v12m6-12v12" stroke-width="2.5"/>',
      next:'<path d="M18 5v14M6 5l10 7-10 7z"/>',
      previous:'<path d="M6 5v14m12-14L8 12l10 7z"/>',
      stop:'<rect x="7" y="7" width="10" height="10" rx="1" fill="currentColor" stroke="none"/>',
      plus:'<path d="M12 5v14M5 12h14"/>',
      close:'<path d="m6 6 12 12M6 18 18 6"/>',
      left:'<path d="m14 5-7 7 7 7"/>',
      right:'<path d="m10 5 7 7-7 7"/>',
      holiday:'<path d="m12 3 6 7h-3l5 7H4l5-7H6l6-7Zm0 14v4"/>',
      music:'<path d="M9 17V5l10-2v12M9 8l10-2"/><ellipse cx="6" cy="18" rx="3" ry="2"/><ellipse cx="16" cy="16" rx="3" ry="2"/>',
      check:'<path d="m5 12 4 4L19 6"/>',
      help:'<circle cx="12" cy="12" r="9"/><path d="M9.5 9a2.5 2.5 0 0 1 5 0c0 2-2.5 2-2.5 4m0 3h.01"/>',
      heart:'<path d="M20 5c-3-3-6-1-8 1-2-2-5-4-8-1-5 5 2 10 8 15 6-5 13-10 8-15z"/>',
      volume:'<path d="M4 9h4l5-4v14l-5-4H4zM17 8a6 6 0 0 1 0 8m3-11a10 10 0 0 1 0 14"/>',
      search:'<circle cx="10" cy="10" r="6"/><path d="m15 15 5 5"/>',
      edit:'<path d="m14 5 5 5M4 20l4-1L20 7l-4-4L4 15z"/>',
      trash:'<path d="M5 6h14M9 6V3h6v3M7 6l1 15h8l1-15m-7 4v7m4-7v7"/>',
      clock:'<circle cx="12" cy="12" r="9"/><path d="M12 7v5l4 2"/>',
      swap:'<path d="M4 8h15l-3-3M20 16H5l3 3"/>',
      account:'<circle cx="12" cy="8" r="4"/><path d="M4 21v-2a8 8 0 0 1 16 0v2"/>'
    };
    return `<svg width="${size}" height="${size}" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true">${paths[name] || paths.music}</svg>`;
  };
  const button = (action, label, {icon, className = 'btn', attrs = ''} = {}) => `<button type="button" class="${className}" data-action="${action}" ${attrs}>${icon ? svg(icon) : ''}<span>${esc(label)}</span></button>`;
  const iconButton = (action, label, icon, attrs = '') => `<button type="button" class="icon-btn" data-action="${action}" aria-label="${esc(label)}" title="${esc(label)}" ${attrs}>${svg(icon)}</button>`;
  const cover = (id, className = '') => `<div class="cover cover-${esc(id)} ${className}" aria-hidden="true"><i></i><span></span></div>`;
  const minutes = value => Number(value.slice(0,2)) * 60 + Number(value.slice(3,5));
  const secondsText = secs => `${String(Math.floor(secs / 60)).padStart(2,'0')}:${String(Math.floor(secs % 60)).padStart(2,'0')}`;
  const dateObject = iso => new Date(iso + 'T12:00:00Z');
  const dateLabel = (iso, options = {}) => new Intl.DateTimeFormat('ru-RU',{timeZone:'UTC',day:'numeric',month:'long',...options}).format(dateObject(iso));
  const isoDate = date => date.toISOString().slice(0,10);
  const addDays = (iso, n) => {const d = dateObject(iso);d.setUTCDate(d.getUTCDate() + n);return isoDate(d);};
  const dayOfWeek = iso => dateObject(iso).getUTCDay() || 7;
  const weekdays = ['Пн','Вт','Ср','Чт','Пт','Сб','Вс'];
  function tracks(prefix, count, seed) {
    return Array.from({length:count}, (_, i) => ({id:prefix+'-'+i,title:seed[i] || `${['Quiet Streets','Paper Moon','Soft Waves','Open Window','Velvet Sky','Evening Walk'][i%6]} ${String(i+1).padStart(2,'0')}`,artist:prefix === 'holiday' ? 'Winter Collection' : 'Soft Current',duration:seed[i] === 'Golden Hour' ? 222 : 180 + (i * 19) % 95}));
  }
  function initialState() {
    return {
      version:3,view:'overview',selectedDate:'2026-12-24',selectedTime:'14:32',month:'2026-12',calendarMode:'month',
      settings:{name:'Кофейня на Мира',zone:'Europe/Moscow',volume:65},
      playlists:[
        {id:'morning',name:'Утро',description:'Мягкое начало дня',tracks:tracks('morning',48,['Morning Paper','First Light','Warm Coffee'])},
        {id:'day',name:'Дневной',description:'Основная программа',tracks:tracks('day',86,['Golden Hour','Slow Motion','Open Window','Daylight'])},
        {id:'evening',name:'Вечер',description:'Спокойный ритм',tracks:tracks('evening',62,['Blue Hour','Afterglow','Quiet Room'])},
        {id:'holiday',name:'Новогодний',description:'Праздничная коллекция',tracks:tracks('holiday',24,['Winter Waltz','December Lights','Winter Prelude','Snowfall'])}
      ],
      favorites:[],
      templates:{weekday:[{start:'08:00',end:'12:00',playlist:'morning'},{start:'12:00',end:'18:00',playlist:'day'},{start:'18:00',end:'22:00',playlist:'evening'}],weekend:[{start:'10:00',end:'13:00',playlist:'morning'},{start:'13:00',end:'19:00',playlist:'day'},{start:'19:00',end:'22:00',playlist:'evening'}]},
      rules:[{id:'new-year',name:'Новогоднее настроение',enabled:true,start:'2026-12-15',end:'2027-01-15',days:[1,2,3,4,5,6,7],exceptions:[],source:'holiday',baseCount:1,holidayCount:1,priority:10}],
      adverts:[{id:'welcome',name:'Добро пожаловать',enabled:true,start:'2026-12-01',end:'2027-01-31',interval:60,duration:20}],
      playback:{playing:false,elapsed:134,current:null,cursors:{},phase:0,history:[],adReturn:null}
    };
  }
  let state;
  try {const saved = JSON.parse(localStorage.getItem(storageKey));state = saved?.version === 3 ? saved : initialState();} catch {state = initialState();}
  if (!['overview','calendar','rules','library','adverts','settings'].includes(state.view)) state.view = 'overview';
  let selectedRuleId = state.rules[0]?.id || null;
  let ruleDraft = selectedRuleId ? clone(state.rules[0]) : null;
  let editorError = '', librarySearch = '', libraryFilter = 'all', modal = null, modalError = '', toastTimer;
  const app = document.getElementById('app');
  const playlist = id => state.playlists.find(p => p.id === id);
  const schedule = (iso = state.selectedDate) => state.templates[dayOfWeek(iso) > 5 ? 'weekend' : 'weekday'];
  const baseAt = (iso = state.selectedDate, time = state.selectedTime) => schedule(iso).find(s => minutes(time) >= minutes(s.start) && minutes(time) < minutes(s.end));
  const matches = (r, iso = state.selectedDate) => r.enabled && iso >= r.start && iso <= r.end && r.days.includes(dayOfWeek(iso)) && !r.exceptions.includes(iso);
  const activeRule = (iso = state.selectedDate) => state.rules.filter(r => matches(r, iso)).sort((a,b) => b.priority - a.priority)[0] || null;
  const visibleRule = () => activeRule() || state.rules.find(r => r.id === selectedRuleId) || state.rules[0] || null;
  function sourcePattern() {
    const base = baseAt()?.playlist;
    if (!base) return [];
    const r = activeRule();
    if (!r || !playlist(r.source)?.tracks.length) return [base];
    return [...Array(r.baseCount).fill(base),...Array(r.holidayCount).fill(r.source)];
  }
  function takeTrack(playback) {
    const pattern = sourcePattern();
    if (!pattern.length) return null;
    let sourceId = pattern[playback.phase % pattern.length];
    playback.phase = (playback.phase + 1) % pattern.length;
    if (!playlist(sourceId)?.tracks.length) sourceId = baseAt()?.playlist;
    const source = playlist(sourceId);
    if (!source?.tracks.length) return null;
    const position = (playback.cursors[sourceId] || 0) % source.tracks.length;
    playback.cursors[sourceId] = position + 1;
    const track = source.tracks[position];
    return {...track,sourceId,sourceTitle:source.name,kind:sourceId === baseAt()?.playlist ? 'base' : 'holiday'};
  }
  function resetPreview() {
    state.playback = {playing:false,elapsed:0,current:null,cursors:{},phase:0,history:[],adReturn:null};
    state.playback.current = takeTrack(state.playback);
  }
  if (!state.playback.current) state.playback.current = takeTrack(state.playback);
  state.playback.playing = false;
  function queue() {
    const preview = clone(state.playback);
    const next = [];
    if (preview.adReturn) next.push(preview.adReturn.current);
    while (next.length < 4) {
      const item = takeTrack(preview);
      if (!item) break;
      next.push(item);
    }
    return next;
  }
  function store() {
    try {localStorage.setItem(storageKey,JSON.stringify(state));} catch {toast('Браузер не разрешил сохранить демо. Изменения доступны до закрытия страницы.');}
  }
  function toast(message) {
    const el = document.getElementById('toast');
    el.textContent = message;el.classList.add('visible');
    clearTimeout(toastTimer);toastTimer = setTimeout(() => el.classList.remove('visible'),4500);
  }
  const panel = (title, body, action = '', className = '') => `<section class="panel ${className}"><div class="panel-head"><h2 class="panel-title">${esc(title)}</h2>${action}</div>${body}</section>`;
  const sourceName = id => playlist(id)?.name || 'Нет источника';
  const field = (name,label,value,{type='text',min,max,step,extra=''}={}) => `<label class="field" for="rule-${name}"><span class="field-label">${esc(label)}</span><input id="rule-${name}" class="field-control" data-rule-field="${name}" type="${type}" value="${esc(value)}" ${min != null ? `min="${min}"` : ''} ${max != null ? `max="${max}"` : ''} ${step != null ? `step="${step}"` : ''} ${extra}></label>`;
  function ruleDirty() {const saved = state.rules.find(r => r.id === selectedRuleId);return !!ruleDraft && JSON.stringify(saved) !== JSON.stringify(ruleDraft);}
  const ruleOptions = value => state.playlists.map(p => `<option value="${esc(p.id)}" ${p.id === value ? 'selected' : ''}>${esc(p.name)} · ${p.tracks.length} треков</option>`).join('');
  function rotation({hero=false}={}) {
    const r = visibleRule();
    if (!r) return panel('Подмешивание',`<div class="empty-state">Создайте правило, чтобы добавлять музыку из другой коллекции.</div>`,button('new-rule','Добавить',{icon:'plus'}));
    const active = matches(r);
    const base = playlist(baseAt()?.playlist || 'day');
    const holiday = playlist(r.source);
    const source = (p,kind,count) => `<button type="button" class="source-card ${kind}" data-action="open-playlist" data-id="${esc(p?.id || '')}"><div class="source-top">${svg(kind === 'holiday' ? 'holiday' : 'music')}<span class="rotation-source-label">${kind === 'holiday' ? 'Дополнительный источник' : 'Текущий по расписанию'}</span></div><h3 class="source-name">${esc(p?.name || 'Нет источника')}</h3><p class="source-count">${p?.tracks.length || 0} треков · ${count} за ход</p></button>`;
    const pattern = [...Array(r.baseCount).fill('base'),...Array(r.holidayCount).fill('holiday')];
    return panel(r.name,
      `${hero ? `<div class="rotation-hero"><div class="rotation-orb" aria-hidden="true"></div><div class="rotation-ratio"><strong>${r.baseCount} : ${r.holidayCount}</strong><span>По очереди</span></div><div class="rotation-orb holiday" aria-hidden="true"></div></div>` : ''}<div class="rotation-sources">${source(base,'base',r.baseCount)}${source(holiday,'holiday',r.holidayCount)}</div><div class="rotation-pattern" aria-label="Порядок источников">${[...pattern,...pattern].slice(0,8).map((s,i) => `${i ? '<span class="pattern-arrow" aria-hidden="true">→</span>' : ''}<span class="pattern-chip ${s}">${s === 'base' ? 'Основной' : 'Праздничный'}</span>`).join('')}</div><div class="rotation-caption"><span>${dateLabel(r.start,{year:'numeric'})} — ${dateLabel(r.end,{year:'numeric'})}</span>${button('edit-rule','Изменить',{className:'btn btn-ghost',icon:'edit',attrs:`data-id="${esc(r.id)}"`})}</div>`,
      `<button type="button" class="badge ${active ? 'badge-main' : ''}" data-action="toggle-rule" data-id="${esc(r.id)}" title="${r.enabled ? 'Выключить правило' : 'Включить правило'}">${r.enabled ? active ? 'Действует в этот день' : 'Вне периода' : 'Выключено'}</button>`, 'rotation-panel');
  }
  function week() {
    const monday = addDays(state.selectedDate,1-dayOfWeek(state.selectedDate));
    return panel('Неделя',`<div class="week-grid">${Array.from({length:7},(_,i) => {const day=addDays(monday,i);const r=activeRule(day);return `<button type="button" class="week-day ${day === state.selectedDate ? 'selected' : ''}" data-action="select-date" data-date="${day}" aria-pressed="${day === state.selectedDate}"><span class="weekday-label">${weekdays[i]}</span><strong>${dateObject(day).getUTCDate()}</strong><span class="calendar-label">${i<5?'Будни':'Выходной'}</span><span class="calendar-rule ${r?'holiday':''}">${r?'Праздник':'Обычная программа'}</span></button>`;}).join('')}</div>`,
      `<div class="calendar-controls">${iconButton('shift-date','Предыдущая неделя','left','data-days="-7"')}<span class="muted">${dateLabel(monday)} — ${dateLabel(addDays(monday,6))}</span>${iconButton('shift-date','Следующая неделя','right','data-days="7"')}</div>`,'week-panel');
  }
  function calendar({compact=false}={}) {
    const first = state.month + '-01';
    const start = addDays(first,1-dayOfWeek(first));
    const last = new Date(Date.UTC(Number(state.month.slice(0,4)),Number(state.month.slice(5)),0));
    const count = Math.ceil((dayOfWeek(first)-1+last.getUTCDate())/7)*7;
    const monthLabel = new Intl.DateTimeFormat('ru-RU',{timeZone:'UTC',month:'long',year:'numeric'}).format(dateObject(first));
    const controls = `<div class="calendar-controls">${iconButton('shift-month','Предыдущий месяц','left','data-step="-1"')}${button('demo-today','Демодата',{className:'btn btn-ghost'})}${iconButton('shift-month','Следующий месяц','right','data-step="1"')}</div>`;
    const modes = !compact ? `<div class="segmented" aria-label="Вид календаря">${['day','week','month'].map((mode,i) => `<button type="button" data-action="calendar-mode" data-mode="${mode}" class="${state.calendarMode===mode?'selected':''}" aria-pressed="${state.calendarMode===mode}">${['День','Неделя','Месяц'][i]}</button>`).join('')}</div>` : '';
    const mode = compact ? 'month' : state.calendarMode;
    let content;
    if (mode === 'month') content = `<div class="calendar-weekdays">${weekdays.map(d=>`<span>${d}</span>`).join('')}</div><div class="calendar-grid ${compact?'compact':''}">${Array.from({length:count},(_,i)=>{const day=addDays(start,i);const r=activeRule(day);const selected=day===state.selectedDate;return `<button type="button" class="calendar-day ${day.slice(0,7)!==state.month?'outside-month':''} ${selected?'selected':''} ${r?'has-rule':''}" data-action="select-date" data-date="${day}" aria-pressed="${selected}" aria-label="${esc(dateLabel(day,{year:'numeric'}))}, ${r?'праздничное подмешивание':'обычная программа'}"><span class="calendar-number">${dateObject(day).getUTCDate()}</span>${!compact?`<span class="calendar-label">${dayOfWeek(day)>5?'Выходной':'Будни'}</span>`:''}<span class="calendar-rule ${r?'holiday':''}">${r?compact?'•':'Новогодний':''}</span></button>`;}).join('')}</div>`;
    else if(mode==='week') content = `<div class="calendar-inline-week">${week()}</div>`;
    else content = `<div class="calendar-day-summary"><p class="muted">${dateLabel(state.selectedDate,{weekday:'long',year:'numeric'})}</p>${program()}</div>`;
    return panel(monthLabel,`${modes}${content}<div class="calendar-caption"><span><i class="legend-dot main"></i>Программа дня</span><span><i class="legend-dot holiday"></i>Подмешивание</span></div>`,controls,'calendar-panel');
  }
  function program() {
    const rows = schedule();
    const active = baseAt();
    return panel('Программа дня',`<div class="program-list">${rows.map((row,i)=>{const p=playlist(row.playlist);return `<button type="button" class="program-row ${row===active?'current':''}" data-action="edit-template" data-index="${i}"><time>${row.start} — ${row.end}</time><span><strong>${esc(p?.name || 'Нет плейлиста')}</strong><small>${esc(p?.description || '')}</small></span>${row===active?'<span class="badge badge-main">Сейчас</span>':svg('edit',15)}</button>`;}).join('')}</div><div class="panel-note">${activeRule()?'Праздничные треки добавляются к текущей программе.':'В этот день действует обычная программа.'}</div>`,button('edit-template','Изменить',{className:'btn btn-ghost',icon:'edit'}),'program-panel');
  }
  function player({compact=false}={}) {
    const t = state.playback.current;
    return panel(compact?'Предпросмотр эфира':'Сейчас в демоэфире',`${t ? `<div class="player-main">${cover(t.sourceId,'player-art')}<div class="player-meta"><h3 class="player-track" data-current-title>${esc(t.title)}</h3><p class="player-subtitle">${esc(t.artist)}</p><span class="badge ${t.kind==='holiday'?'badge-holiday':'badge-main'}" data-current-source>${esc(t.sourceTitle)}</span></div></div><div class="player-progress"><input type="range" class="seek-control" data-action="seek" aria-label="Позиция трека" min="0" max="${t.duration}" value="${Math.min(state.playback.elapsed,t.duration)}"><div class="player-time"><span data-elapsed>${secondsText(state.playback.elapsed)}</span><span>${secondsText(t.duration)}</span></div></div><div class="player-controls">${iconButton('previous','Предыдущий трек','previous')}${iconButton('play-toggle',state.playback.playing?'Пауза':'Воспроизвести',state.playback.playing?'pause':'play')}${iconButton('next','Следующий трек','next')}${iconButton('stop','Остановить','stop')}</div>` : '<div class="empty-state">На выбранное время музыка не назначена. Выберите время внутри программы дня.</div>'}<label class="volume-control">${svg('volume',16)}<input type="range" data-action="volume" aria-label="Громкость" min="0" max="100" value="${state.settings.volume}"><span data-volume>${state.settings.volume}%</span></label><p class="panel-note">Кнопки управляют демонстрацией. Звук не воспроизводится.</p>`,
      `<span class="badge">${state.playback.playing?'Идёт демонстрация':'Пауза'}</span>`, `player-panel ${compact?'compact-player':''}`);
  }
  function queueComponent({strip=false}={}) {
    const current=state.playback.current;
    const items=current?[current,...queue()]:[];
    return panel('Очередь эфира',items.length?`<div class="queue-list ${strip?'queue-strip':''}">${items.map((t,i)=>`<button type="button" class="queue-row ${i===0?'current':''}" data-action="queue-play" data-index="${i}" data-source="${t.kind}" title="${i===0?'Текущий трек':'Перейти к этому треку'}">${strip?cover(t.sourceId):`<span class="queue-number">${i===0?svg('play',12):String(i).padStart(2,'0')}</span>`}<span class="queue-title">${esc(t.title)}<small class="queue-source ${t.kind==='holiday'?'holiday':''}">${esc(t.sourceTitle)}${i===0?' · сейчас':''}</small></span><span class="queue-duration">${secondsText(t.duration)}</span></button>`).join('')}</div>`:'<div class="empty-state">Очередь появится, когда на выбранное время будет назначен плейлист.</div>',
      `<span class="badge">${activeRule()?`${activeRule().baseCount} : ${activeRule().holidayCount}`:'Основная программа'}</span>`, 'queue-panel');
  }
  function library({compact=false}={}) {
    const list=state.playlists.filter(p=>(libraryFilter!=='favorites'||state.favorites.includes(p.id)) && `${p.name} ${p.description} ${p.tracks.map(t=>t.title).join(' ')}`.toLowerCase().includes(librarySearch.toLowerCase()));
    const tools=`<div class="toolbar"><label class="search-field">${svg('search',17)}<input type="search" data-library-search value="${esc(librarySearch)}" placeholder="Найти плейлист или трек" aria-label="Поиск в медиатеке"></label><div class="segmented">${['all','favorites'].map((filter,i)=>`<button type="button" data-action="library-filter" data-filter="${filter}" class="${libraryFilter===filter?'selected':''}" aria-pressed="${libraryFilter===filter}">${['Все','Избранное'][i]}</button>`).join('')}</div></div>`;
    return panel('Коллекции',`${tools}<div class="library-grid ${compact?'compact-library':''}" data-library-grid>${list.map(p=>`<article class="playlist-card"><button type="button" class="playlist-open" data-action="open-playlist" data-id="${esc(p.id)}" aria-label="Открыть плейлист ${esc(p.name)}">${cover(p.id)}<h3>${esc(p.name)}</h3><p>${p.tracks.length} треков · ${esc(p.description)}</p></button><div class="playlist-actions"><span class="badge ${p.id==='holiday'?'badge-holiday':'badge-main'}">${p.id==='holiday'?'Дополнительный':'Основной'}</span><button type="button" class="icon-btn ${state.favorites.includes(p.id)?'is-favorite':''}" data-action="favorite" data-id="${esc(p.id)}" aria-label="${state.favorites.includes(p.id)?'Убрать из избранного':'В избранное'}" aria-pressed="${state.favorites.includes(p.id)}">${svg('heart',16)}</button></div></article>`).join('') || '<div class="empty-state">Ничего не найдено. Измените запрос или добавьте коллекцию.</div>'}</div>`,button('new-playlist','Добавить',{icon:'plus'}),'library-panel');
  }
  function ruleList() {
    return panel('Правила',`<div class="rule-list">${state.rules.map(r=>`<div class="rule-row ${r.id===selectedRuleId?'selected':''}"><button type="button" class="rule-select" data-action="select-rule" data-id="${esc(r.id)}"><strong>${esc(r.name)}</strong><small>${r.baseCount}:${r.holidayCount} · ${sourceName(r.source)}</small><span class="rule-status">${matches(r)?'Действует в выбранный день':r.enabled?'По календарю':'Выключено'}</span></button><button type="button" class="toggle ${r.enabled?'on':''}" data-action="toggle-rule" data-id="${esc(r.id)}" aria-label="Включить правило ${esc(r.name)}" aria-pressed="${r.enabled}"><span class="toggle-track"></span></button></div>`).join('') || '<div class="empty-state">Пока нет правил подмешивания.</div>'}</div>`,button('new-rule','Новое',{icon:'plus',className:'btn btn-ghost'}),'rule-list-panel');
  }
  function ruleEditor() {
    const r=ruleDraft;
    if (!r) return panel('Редактор правила','<div class="empty-state">Выберите правило слева или создайте новое.</div>',button('new-rule','Создать',{icon:'plus'}));
    const countOptions=value=>[1,2,3].map(n=>`<option value="${n}" ${Number(value)===n?'selected':''}>${n} ${n===1?'трек':'трека'}</option>`).join('');
    return panel('Настройка подмешивания',`<form data-rule-form novalidate><div class="field-grid">${field('name','Название правила',r.name,{extra:'maxlength="80"'})}${field('priority','Приоритет',r.priority,{type:'number',min:1,max:100})}${field('start','С какой даты',r.start,{type:'date'})}${field('end','По какую дату включительно',r.end,{type:'date'})}<label class="field" for="rule-source"><span class="field-label">Дополнительный плейлист</span><select id="rule-source" class="field-control" data-rule-field="source">${ruleOptions(r.source)}</select></label><div class="field"><span class="field-label">Основной источник</span><div class="field-control static-value">Текущий по расписанию дня</div></div><label class="field" for="rule-baseCount"><span class="field-label">Основных треков за ход</span><select id="rule-baseCount" class="field-control" data-rule-field="baseCount">${countOptions(r.baseCount)}</select></label><label class="field" for="rule-holidayCount"><span class="field-label">Дополнительных треков за ход</span><select id="rule-holidayCount" class="field-control" data-rule-field="holidayCount">${countOptions(r.holidayCount)}</select></label></div><div class="rule-options"><span class="field-label">В какие дни недели</span><div class="weekday-picker">${weekdays.map((d,i)=>`<button type="button" class="weekday-btn ${r.days.includes(i+1)?'selected':''}" data-action="draft-day" data-day="${i+1}" aria-pressed="${r.days.includes(i+1)}">${d}</button>`).join('')}</div></div><div class="rotation-pattern">${[...Array(Number(r.baseCount)).fill('Основной'),...Array(Number(r.holidayCount)).fill('Праздничный')].map((s,i)=>`${i?'<span class="pattern-arrow">→</span>':''}<span class="pattern-chip ${s==='Основной'?'base':'holiday'}">${s}</span>`).join('')}<span class="muted">→ повтор</span></div><div class="exception-line"><span class="muted">Исключённые даты: ${r.exceptions.length?r.exceptions.map(d=>dateLabel(d)).join(', '):'нет'}</span>${button('edit-exceptions','Исключения',{className:'btn btn-ghost',icon:'calendar'})}</div><label class="check-line"><input type="checkbox" data-rule-field="enabled" ${r.enabled?'checked':''}>Правило включено</label><p class="panel-note">Каждый список продолжает свою очередь. Реклама не меняет порядок музыкального чередования.</p><div class="error ${editorError?'':'hidden'}" role="alert" data-editor-error>${esc(editorError)}</div><div class="editor-actions">${button('save-rule','Сохранить правило',{className:'btn btn-primary',icon:'check'})}${button('cancel-rule','Отменить изменения')}${state.rules.some(s=>s.id===r.id)?button('delete-rule','Удалить',{className:'btn btn-ghost',icon:'trash'}):''}</div></form>`,
      `<span class="badge ${ruleDirty()?'badge-holiday':'badge-main'}" data-draft-status>${ruleDirty()?'Есть изменения':'Сохранено'}</span>`, 'rule-editor-panel');
  }
  function inspector() {
    const r=activeRule();
    return panel(dateLabel(state.selectedDate),`<p class="eyebrow">${dateLabel(state.selectedDate,{weekday:'long',year:'numeric'})}</p><div class="inspector-program">${program()}</div>${r?`<div class="inspector-rule"><span class="badge badge-holiday">Активное подмешивание</span><h3>${esc(r.name)}</h3><p>${esc(sourceName(r.source))} · ${r.baseCount}:${r.holidayCount}</p>${button('edit-rule','Открыть правило',{className:'btn btn-ghost',icon:'right',attrs:`data-id="${esc(r.id)}"`})}</div>`:'<div class="empty-state">Праздничных правил на эту дату нет. Звучит обычная программа.</div>'}<div class="inspector-actions">${button('exception-selected','Исключить этот день',{className:'btn btn-ghost',icon:'calendar'})}</div>`,'','inspector-panel');
  }
  function timeline() {
    const rows=schedule(), r=activeRule(), active=baseAt();
    const tokens=r?[...Array(r.baseCount).fill('base'),...Array(r.holidayCount).fill('holiday')]:[];
    return panel('Временная шкала дня',`<div class="timeline-grid"><div class="timeline-axis-label muted">Время</div><div class="muted">Основная программа</div><div class="muted">Подмешивание</div>${rows.map((row,i)=>`<time class="timeline-time">${row.start}</time><button type="button" class="timeline-block ${row===active?'current':''}" data-action="edit-template" data-index="${i}"><span>${row.start} — ${row.end}</span><strong>${esc(sourceName(row.playlist))}</strong><small>${esc(playlist(row.playlist)?.description || '')}</small>${row===active?`<span class="badge badge-main">${state.selectedTime} · выбрано сейчас</span>`:''}</button>${i===0?`<button type="button" class="timeline-overlay ${r?'active':''}" data-action="${r?'edit-rule':'new-rule'}" ${r?`data-id="${esc(r.id)}"`:''}><span class="badge ${r?'badge-holiday':''}">${r?'Активно':'Нет правила'}</span><strong>${r?esc(sourceName(r.source)):'Добавить подмешивание'}</strong><small>${r?`${r.baseCount} основной → ${r.holidayCount} праздничный`:'Для праздников и особых дат'}</small><div class="timeline-rotation" aria-hidden="true">${r?Array.from({length:6},(_,n)=>`<i class="${tokens[n%tokens.length]}">${tokens[n%tokens.length]==='holiday'?'П':'О'}</i>`).join(''):svg('plus',24)}</div></button>`:''}`).join('')}<time class="timeline-time">${rows.at(-1).end}</time></div><p class="panel-note">Нажмите на программу, чтобы изменить её часы. Праздничный слой следует своему календарю.</p>`,button('edit-template','Изменить программу',{className:'btn btn-ghost',icon:'edit'}),'timeline-panel');
  }
  function adverts() {
    return panel('Рекламные вставки',`<p class="panel-note">Вставки звучат отдельно и сохраняют очередность основных и праздничных треков.</p><div class="advert-list">${state.adverts.map(a=>`<div class="list-item"><div><strong>${esc(a.name)}</strong><p class="muted">Каждые ${a.interval} мин · ${a.duration} сек · ${dateLabel(a.start)} — ${dateLabel(a.end)}</p></div><div class="list-actions"><button type="button" class="toggle ${a.enabled?'on':''}" data-action="toggle-advert" data-id="${esc(a.id)}" aria-label="Включить вставку ${esc(a.name)}" aria-pressed="${a.enabled}"><span class="toggle-track"></span></button>${iconButton('play-advert','Проиграть в демо','play',`data-id="${esc(a.id)}"`)}${iconButton('edit-advert','Изменить вставку','edit',`data-id="${esc(a.id)}"`)}${iconButton('delete-advert','Удалить вставку','trash',`data-id="${esc(a.id)}"`)}</div></div>`).join('') || '<div class="empty-state">Пока нет рекламных вставок.</div>'}</div>`,button('new-advert','Добавить вставку',{icon:'plus'}),'adverts-panel');
  }
  function settings() {
    return panel('Пространство',`<form data-settings-form class="settings-form"><div class="field-grid"><label class="field" for="station-name"><span class="field-label">Название пространства</span><input class="field-control" id="station-name" name="name" value="${esc(state.settings.name)}" maxlength="80" required></label><label class="field" for="station-zone"><span class="field-label">Время станции</span><select class="field-control" id="station-zone" name="zone"><option value="Europe/Moscow" ${state.settings.zone==='Europe/Moscow'?'selected':''}>Москва · UTC+3</option><option value="UTC" ${state.settings.zone==='UTC'?'selected':''}>UTC</option><option value="Asia/Yekaterinburg" ${state.settings.zone==='Asia/Yekaterinburg'?'selected':''}>Екатеринбург · UTC+5</option></select></label></div><label class="volume-control">Громкость<input type="range" data-action="volume" aria-label="Громкость" min="0" max="100" value="${state.settings.volume}"><span data-volume>${state.settings.volume}%</span></label><div class="editor-actions"><button class="btn btn-primary" type="submit">${svg('check')}Сохранить настройки</button>${button('reset-demo','Сбросить демо',{className:'btn btn-ghost'})}</div></form><p class="panel-note">Настройки и правила сохраняются в этом браузере отдельно для каждого варианта.</p>`,'','settings-panel');
  }
  const ui={week,calendar,program,player,rotation,queue:queueComponent,library,ruleEditor,ruleList,inspector,timeline,adverts,settings};
  function viewContent() {
    if(state.view==='overview') return window.LampBoxLayouts[variant](ui);
    if(state.view==='calendar') return `<div class="calendar-workspace">${calendar()}${inspector()}</div>${player({compact:true})}`;
    if(state.view==='rules') return `<div class="rules-workspace">${ruleList()}${ruleEditor()}</div>${queueComponent({strip:true})}`;
    if(state.view==='library') return library();
    if(state.view==='adverts') return `<div class="two-column">${adverts()}<div>${player()}${queueComponent()}</div></div>`;
    return settings();
  }
  function render() {
    const active=document.activeElement;
    const focusId=active?.id;
    let caret;try {caret=active?.selectionStart;} catch {}
    const nav=[['overview','Обзор'],['calendar','Календарь'],['rules','Правила'],['library','Медиатека'],['adverts','Реклама'],['settings','Настройки']];
    const title=state.view==='overview'?meta[2]:nav.find(n=>n[0]===state.view)[1];
    app.innerHTML=`<div class="prototype-bar"><a href="index.html">← Все варианты</a><span>${meta[0]} / ${meta[1]} · Интерактивный прототип</span>${button('help','Что попробовать',{className:'btn btn-ghost',icon:'help'})}</div><div class="app-shell"><header class="app-topbar"><button type="button" class="app-brand" data-nav="overview" aria-label="MediaBoxManager — обзор"><span class="brand-mark" aria-hidden="true"></span>MediaBoxManager</button><div class="topbar-context"><span class="status-dot"></span>Демонстрация<span class="topbar-separator"></span><button type="button" class="station-button" data-action="station-info">${esc(state.settings.name)}</button></div>${iconButton('help','Как пользоваться прототипом','help')}<button type="button" class="avatar-button" data-action="station-info" aria-label="Настройки пространства">АМ</button></header><nav class="app-nav" aria-label="Разделы MediaBoxManager">${nav.map(([key,label])=>`<button type="button" class="nav-item ${state.view===key?'selected':''}" data-nav="${key}" aria-current="${state.view===key?'page':'false'}" title="${label}">${svg(key)}<span>${label}</span></button>`).join('')}<div class="nav-footer"><span class="status-dot"></span>Данные демо сохранены локально</div></nav><main class="app-content"><div class="page-heading"><div><p class="eyebrow">${state.view==='overview'?'Музыка вашего пространства':'Ваше пространство'}</p><h1>${esc(title)}</h1><p class="page-subtitle">${esc(state.view==='overview'?meta[3]:'Проверьте сценарий на демонстрационных данных.')}</p></div><div class="preview-clock"><label for="preview-date"><span>Дата предпросмотра</span><input id="preview-date" type="date" data-action="preview-date" value="${state.selectedDate}" aria-label="Дата предпросмотра"></label><label for="preview-time"><span>Время</span><input id="preview-time" type="time" data-action="preview-time" value="${state.selectedTime}" aria-label="Время предпросмотра"></label></div></div><div class="view-content" data-view="${state.view}">${viewContent()}</div><footer class="app-footer"><span>Демоданные · ${dateLabel(state.selectedDate,{year:'numeric'})} · ${state.selectedTime}</span><span>${state.settings.zone} · ${state.playback.playing?'Очередь проигрывается без звука':'Нажмите Play, чтобы запустить демонстрацию'}</span></footer></main></div>`;
    if(focusId){const el=document.getElementById(focusId);el?.focus({preventScroll:true});if(caret!=null&&el?.setSelectionRange){try{el.setSelectionRange(caret,caret);}catch{}}}
    renderModal();
  }
  function setDate(day) {state.selectedDate=day;state.month=day.slice(0,7);resetPreview();store();render();}
  function nextTrack() {
    if(!state.playback.current) return;
    if(state.playback.adReturn){Object.assign(state.playback,state.playback.adReturn,{adReturn:null});return;}
    state.playback.history.push({current:clone(state.playback.current),elapsed:state.playback.elapsed,cursors:clone(state.playback.cursors),phase:state.playback.phase});
    if(state.playback.history.length>30) state.playback.history.shift();
    state.playback.current=takeTrack(state.playback);state.playback.elapsed=0;
  }
  function selectRule(id) {
    selectedRuleId=id;ruleDraft=clone(state.rules.find(r=>r.id===id));editorError='';
  }
  function syncPattern() {
    const pattern=sourcePattern();
    if(!pattern.length) return;
    const index=pattern.indexOf(state.playback.current?.sourceId);
    state.playback.phase=index>=0?(index+1)%pattern.length:0;
  }
  function saveRule() {
    if(!ruleDraft) return;
    const r=clone(ruleDraft);
    let error='';
    if(!r.name.trim()) error='Введите название правила.';
    else if(!/^\d{4}-\d{2}-\d{2}$/.test(r.start)||!/^\d{4}-\d{2}-\d{2}$/.test(r.end)) error='Укажите обе даты.';
    else if(r.start>r.end) error='Начало периода должно быть раньше окончания.';
    else if(!r.days.length) error='Выберите хотя бы один день недели.';
    else if(!Number.isInteger(r.priority)||r.priority<1||r.priority>100) error='Приоритет должен быть целым числом от 1 до 100.';
    else if(!playlist(r.source)) error='Выберите дополнительный плейлист.';
    if(error){editorError=error;render();document.querySelector('[data-editor-error]')?.scrollIntoView({block:'nearest'});return;}
    const index=state.rules.findIndex(s=>s.id===r.id);
    if(index<0) state.rules.push(r);else state.rules[index]=r;
    selectedRuleId=r.id;ruleDraft=clone(r);editorError='';syncPattern();store();render();toast('Правило сохранено. Очередь следующих треков обновлена.');
  }
  function markDirty() {document.querySelectorAll('[data-draft-status]').forEach(el=>{el.textContent=ruleDirty()?'Есть изменения':'Сохранено';el.classList.toggle('badge-holiday',ruleDirty());el.classList.toggle('badge-main',!ruleDirty());});}
  function openModal(type,data={}) {modal={type,...data};modalError='';renderModal();}
  function closeModal() {const dialog=document.getElementById('modal');dialog.close();modal=null;modalError='';}
  function modalContent() {
    if(modal.type==='help') return {title:'Попробуйте настоящий сценарий',body:`<ol class="help-list"><li><strong>Выберите дату.</strong><p>24 декабря действует новогоднее правило. 16 января праздничное подмешивание уже выключено.</p></li><li><strong>Измените чередование.</strong><p>В «Правилах» выберите два основных трека за ход и сохраните. Очередь станет 2:1.</p></li><li><strong>Нажмите Play и «Следующий».</strong><p>Прогресс и очередь меняются. Музыкальные файлы не воспроизводятся.</p></li><li><strong>Откройте медиатеку.</strong><p>Найдите трек, добавьте коллекцию в избранное или создайте собственный плейлист.</p></li><li><strong>Проверьте рекламу.</strong><p>Запустите вставку в демо. После неё продолжится музыкальная очередь.</p></li></ol><p class="panel-note">Изменения остаются в вашем браузере. В настройках есть сброс демо.</p>`,footer:button('close-modal','Понятно',{className:'btn btn-primary'})};
    if(modal.type==='station') return {title:state.settings.name,body:`<div class="station-summary"><span class="badge badge-main">Демонстрационное пространство</span><p>Время: ${esc(state.settings.zone)}</p><p>Громкость: ${state.settings.volume}%</p><p>Коллекций: ${state.playlists.length} · Правил: ${state.rules.length}</p></div><p class="panel-note">Этот прототип работает с демоданными и не управляет установленным LampPlayer.</p>`,footer:button('open-settings','Настроить пространство',{className:'btn btn-primary'})};
    if(modal.type==='playlist') {
      const p=playlist(modal.id),query=modal.query || '';
      const all=p.tracks.map((t,i)=>({...t,position:i})).filter(t=>`${t.title} ${t.artist}`.toLowerCase().includes(query.toLowerCase()));
      return {title:p.name,body:`<div class="playlist-detail-header">${cover(p.id)}<div><span class="badge">${p.tracks.length} треков</span><p class="muted">${esc(p.description)}</p></div></div><label class="search-field">${svg('search',17)}<input id="playlist-search" type="search" data-playlist-search value="${esc(query)}" placeholder="Поиск в плейлисте" aria-label="Поиск в плейлисте"></label><div class="track-table">${all.slice(0,modal.limit||10).map(t=>`<div class="list-item"><div><strong>${esc(t.title)}</strong><small class="muted">${esc(t.artist)} · ${secondsText(t.duration)}</small></div>${iconButton('preview-track','Включить этот трек в демоэфире','play',`data-id="${p.id}" data-position="${t.position}"`)}</div>`).join('') || '<div class="empty-state">В этой коллекции пока нет подходящих треков.</div>'}</div>${all.length>(modal.limit||10)?button('more-tracks','Показать ещё',{className:'btn btn-ghost'}):''}`,footer:`${button('import-tracks','Добавить аудиофайлы',{icon:'plus'})}${button('close-modal','Готово',{className:'btn btn-primary'})}`};
    }
    if(modal.type==='new-playlist') return {title:'Новая коллекция',body:`<form id="new-playlist-form"><label class="field" for="playlist-name"><span class="field-label">Название плейлиста</span><input class="field-control" id="playlist-name" name="name" placeholder="Например, зимние вечера" required maxlength="80"></label><label class="field" for="playlist-description"><span class="field-label">Описание</span><input class="field-control" id="playlist-description" name="description" placeholder="Какое настроение создаёт музыка" maxlength="120"></label><p class="panel-note">После создания можно добавить файлы. В демо сохраняются только их названия.</p></form>`,footer:`${button('close-modal','Отмена')}<button type="submit" form="new-playlist-form" class="btn btn-primary">Создать коллекцию</button>`};
    if(modal.type==='exceptions') return {title:'Исключения из правила',body:`<p class="muted">В эти даты праздничные треки не будут подмешиваться.</p><div class="exception-list">${ruleDraft.exceptions.map(d=>`<div class="list-item"><span>${dateLabel(d,{year:'numeric'})}</span>${iconButton('remove-exception','Убрать исключение','close',`data-date="${d}"`)}</div>`).join('') || '<div class="empty-state">Исключений пока нет.</div>'}</div><form id="exception-form" class="inline-form"><label class="field" for="exception-date"><span class="field-label">Добавить дату</span><input id="exception-date" class="field-control" name="date" type="date" value="${state.selectedDate}" required></label><button type="submit" class="btn">Добавить</button></form>`,footer:button('close-modal','Готово',{className:'btn btn-primary'})};
    if(modal.type==='template') {
      const kind=dayOfWeek(state.selectedDate)>5?'weekend':'weekday';
      const rows=modal.templateDraft || state.templates[kind];
      return {title:kind==='weekend'?'Программа выходного дня':'Программа рабочего дня',body:`<form id="template-form"><p class="panel-note">Изменения применятся ко всем ${kind==='weekend'?'выходным':'рабочим'} дням. Интервалы не должны пересекаться.</p><div class="template-editor">${rows.map((row,i)=>`<div class="template-row"><label class="field"><span class="field-label">Начало</span><input class="field-control" name="start-${i}" type="time" value="${row.start}" required></label><label class="field"><span class="field-label">Окончание</span><input class="field-control" name="end-${i}" type="time" value="${row.end}" required></label><label class="field"><span class="field-label">Плейлист</span><select class="field-control" name="playlist-${i}">${ruleOptions(row.playlist)}</select></label></div>`).join('')}</div></form>`,footer:`${button('close-modal','Отмена')}<button type="submit" form="template-form" class="btn btn-primary">Сохранить программу</button>`};
    }
    if(modal.type==='advert') {
      const a=modal.draft;
      return {title:modal.new?'Новая рекламная вставка':'Настройка вставки',body:`<form id="advert-form"><div class="field-grid"><label class="field"><span class="field-label">Название</span><input class="field-control" name="name" value="${esc(a.name)}" required maxlength="80"></label><label class="field"><span class="field-label">Периодичность, минут</span><input class="field-control" name="interval" type="number" min="1" max="1440" value="${a.interval}" required></label><label class="field"><span class="field-label">С какой даты</span><input class="field-control" name="start" type="date" value="${a.start}" required></label><label class="field"><span class="field-label">По какую дату</span><input class="field-control" name="end" type="date" value="${a.end}" required></label><label class="field"><span class="field-label">Длительность демо, секунд</span><input class="field-control" name="duration" type="number" min="1" max="300" value="${a.duration}" required></label><label class="check-line"><input type="checkbox" name="enabled" ${a.enabled?'checked':''}>Вставка включена</label></div></form><p class="panel-note">Запуск вставки можно проверить кнопкой Play в списке рекламы.</p>`,footer:`${button('close-modal','Отмена')}<button type="submit" form="advert-form" class="btn btn-primary">Сохранить вставку</button>`};
    }
    if(modal.type==='delete-rule') return {title:'Удалить правило?',body:`<p>«${esc(ruleDraft?.name)}» будет удалено из этого демоварианта. Музыкальные коллекции сохранятся.</p>`,footer:`${button('close-modal','Отмена')}${button('confirm-delete-rule','Удалить правило',{className:'btn btn-primary'})}`};
    return {title:'Сбросить демонстрацию?',body:'<p>Даты, правила, коллекции и настройки этого варианта вернутся к исходному примеру.</p>',footer:`${button('close-modal','Отмена')}${button('confirm-reset','Сбросить демо',{className:'btn btn-primary'})}`};
  }
  function renderModal() {
    const dialog=document.getElementById('modal');
    if(!modal){if(dialog.open)dialog.close();return;}
    const focused=document.activeElement?.id;
    let caret;try{caret=document.activeElement?.selectionStart;}catch{}
    const content=modalContent();
    dialog.innerHTML=`<div class="modal-header"><h2 id="modal-title">${esc(content.title)}</h2>${iconButton('close-modal','Закрыть окно','close')}</div><div class="modal-body">${content.body}<div class="error ${modalError?'':'hidden'}" role="alert">${esc(modalError)}</div></div><div class="modal-footer">${content.footer}</div>`;
    if(!dialog.open)dialog.showModal();
    if(focused){const el=document.getElementById(focused);el?.focus({preventScroll:true});if(caret!=null&&el?.setSelectionRange){try{el.setSelectionRange(caret,caret);}catch{}}}
  }
  function changeView(view) {state.view=view;store();render();window.scrollTo({top:0,behavior:'instant'});}
  document.addEventListener('click',event=>{
    const nav=event.target.closest('[data-nav]');if(nav){changeView(nav.dataset.nav);return;}
    const el=event.target.closest('[data-action]');if(!el || el.matches('input'))return;
    const {action,id}=el.dataset;
    if(action==='help')return openModal('help');
    if(action==='close-modal')return closeModal();
    if(action==='station-info')return openModal('station');
    if(action==='open-settings'){closeModal();return changeView('settings');}
    if(action==='select-date')return setDate(el.dataset.date);
    if(action==='shift-date')return setDate(addDays(state.selectedDate,Number(el.dataset.days)));
    if(action==='demo-today')return setDate('2026-12-24');
    if(action==='shift-month'){const d=dateObject(state.month+'-01');d.setUTCMonth(d.getUTCMonth()+Number(el.dataset.step));state.month=isoDate(d).slice(0,7);store();return render();}
    if(action==='calendar-mode'){state.calendarMode=el.dataset.mode;store();return render();}
    if(action==='play-toggle'){if(!state.playback.current)return toast('Выберите время внутри музыкальной программы.');state.playback.playing=!state.playback.playing;store();render();return toast(state.playback.playing?'Демонстрация запущена. Прогресс идёт без звука.':'Демонстрация на паузе.');}
    if(action==='next'){nextTrack();store();return render();}
    if(action==='previous'){const previous=state.playback.history.pop();if(!previous)return toast('Это первый трек демонстрации.');Object.assign(state.playback,previous,{adReturn:null});store();return render();}
    if(action==='stop'){state.playback.playing=false;state.playback.elapsed=0;store();render();return toast('Демонстрация остановлена.');}
    if(action==='queue-play'){for(let i=0;i<Number(el.dataset.index);i++)nextTrack();store();render();return toast(Number(el.dataset.index)?'Очередь продолжится с выбранного трека.':'Это текущий трек.');}
    if(action==='open-playlist')return openModal('playlist',{id,limit:10,query:''});
    if(action==='more-tracks'){modal.limit=(modal.limit||10)+10;return renderModal();}
    if(action==='favorite'){state.favorites=state.favorites.includes(id)?state.favorites.filter(x=>x!==id):[...state.favorites,id];store();render();return toast(state.favorites.includes(id)?'Коллекция добавлена в избранное.':'Коллекция убрана из избранного.');}
    if(action==='library-filter'){libraryFilter=el.dataset.filter;return render();}
    if(action==='new-playlist')return openModal('new-playlist');
    if(action==='import-tracks'){document.getElementById('file-import').click();return;}
    if(action==='preview-track'){const p=playlist(id), t=p.tracks[Number(el.dataset.position)];state.playback.current={...t,sourceId:id,sourceTitle:p.name,kind:id===baseAt()?.playlist?'base':'holiday'};state.playback.elapsed=0;state.playback.cursors[id]=Number(el.dataset.position)+1;syncPattern();store();closeModal();render();return toast('Выбранный трек поставлен в демонстрационный эфир.');}
    if(action==='new-rule'){selectedRuleId='rule-'+Date.now();ruleDraft={id:selectedRuleId,name:'Новое подмешивание',enabled:true,start:state.selectedDate,end:addDays(state.selectedDate,7),days:[1,2,3,4,5,6,7],exceptions:[],source:'holiday',baseCount:1,holidayCount:1,priority:Math.min(100,Math.max(0,...state.rules.map(r=>r.priority))+10)};editorError='';return changeView('rules');}
    if(action==='select-rule'){selectRule(id);return render();}
    if(action==='edit-rule'){selectRule(id);return changeView('rules');}
    if(action==='toggle-rule'){const r=state.rules.find(r=>r.id===id);r.enabled=!r.enabled;if(selectedRuleId===id)ruleDraft=clone(r);syncPattern();store();render();return toast(r.enabled?'Правило включено.':'Правило выключено.');}
    if(action==='draft-day'){const day=Number(el.dataset.day);ruleDraft.days=ruleDraft.days.includes(day)?ruleDraft.days.filter(d=>d!==day):[...ruleDraft.days,day].sort();return render();}
    if(action==='save-rule')return saveRule();
    if(action==='cancel-rule'){const saved=state.rules.find(r=>r.id===selectedRuleId);if(saved)ruleDraft=clone(saved);else{selectedRuleId=state.rules[0]?.id||null;ruleDraft=state.rules[0]?clone(state.rules[0]):null;}editorError='';render();return toast('Изменения отменены.');}
    if(action==='delete-rule')return openModal('delete-rule');
    if(action==='confirm-delete-rule'){state.rules=state.rules.filter(r=>r.id!==selectedRuleId);selectedRuleId=state.rules[0]?.id||null;ruleDraft=state.rules[0]?clone(state.rules[0]):null;syncPattern();store();closeModal();render();return toast('Правило удалено.');}
    if(action==='edit-exceptions')return openModal('exceptions');
    if(action==='remove-exception'){ruleDraft.exceptions=ruleDraft.exceptions.filter(d=>d!==el.dataset.date);markDirty();return renderModal();}
    if(action==='exception-selected'){const r=activeRule();if(!r)return toast('На эту дату нет активного праздничного правила.');selectRule(r.id);if(!ruleDraft.exceptions.includes(state.selectedDate))ruleDraft.exceptions.push(state.selectedDate);state.view='rules';render();return toast('Дата добавлена в исключения черновика. Сохраните правило, чтобы применить.');}
    if(action==='edit-template')return openModal('template');
    if(action==='new-advert')return openModal('advert',{new:true,draft:{id:'advert-'+Date.now(),name:'Новая вставка',enabled:true,start:state.selectedDate,end:addDays(state.selectedDate,30),interval:60,duration:20}});
    if(action==='edit-advert')return openModal('advert',{new:false,draft:clone(state.adverts.find(a=>a.id===id))});
    if(action==='toggle-advert'){const a=state.adverts.find(a=>a.id===id);a.enabled=!a.enabled;store();return render();}
    if(action==='delete-advert'){state.adverts=state.adverts.filter(a=>a.id!==id);store();render();return toast('Вставка удалена.');}
    if(action==='play-advert'){const a=state.adverts.find(a=>a.id===id);if(!a.enabled||state.selectedDate<a.start||state.selectedDate>a.end)return toast('Вставка выключена или находится вне своего периода.');if(!state.playback.current)return toast('Сначала выберите время внутри программы дня.');if(state.playback.adReturn)return toast('Демонстрационная вставка уже запущена.');state.playback.adReturn={current:clone(state.playback.current),elapsed:state.playback.elapsed};state.playback.current={id:a.id,title:a.name,artist:'Рекламная вставка',duration:a.duration,sourceId:'advert',sourceTitle:'Реклама',kind:'advert'};state.playback.elapsed=0;state.playback.playing=true;store();render();return toast('Идёт демовставка. Затем музыка продолжится с прежнего места.');}
    if(action==='reset-demo')return openModal('reset');
    if(action==='confirm-reset'){state=initialState();selectedRuleId=state.rules[0].id;ruleDraft=clone(state.rules[0]);state.playback.current=takeTrack(state.playback);librarySearch='';libraryFilter='all';editorError='';store();closeModal();render();return toast('Демонстрация сброшена.');}
  });
  document.addEventListener('input',event=>{
    const el=event.target;
    if(el.dataset.ruleField){const key=el.dataset.ruleField;ruleDraft[key]=el.type==='checkbox'?el.checked:['priority','baseCount','holidayCount'].includes(key)?Number(el.value):el.value;editorError='';markDirty();if(['baseCount','holidayCount','source'].includes(key))render();return;}
    if(el.hasAttribute('data-library-search')){librarySearch=el.value;const caret=el.selectionStart;render();const input=document.querySelector('[data-library-search]');input.focus();input.setSelectionRange(caret,caret);return;}
    if(el.hasAttribute('data-playlist-search')){modal.query=el.value;modal.limit=10;return renderModal();}
    if(el.dataset.action==='seek'){state.playback.elapsed=Number(el.value);document.querySelectorAll('[data-elapsed]').forEach(n=>n.textContent=secondsText(state.playback.elapsed));store();return;}
    if(el.dataset.action==='volume'){state.settings.volume=Number(el.value);document.querySelectorAll('[data-volume]').forEach(n=>n.textContent=state.settings.volume+'%');store();}
  });
  document.addEventListener('change',event=>{
    const el=event.target;
    if(el.dataset.action==='preview-date'&&el.value)return setDate(el.value);
    if(el.dataset.action==='preview-time'&&el.value){state.selectedTime=el.value;resetPreview();store();return render();}
    if(el.id==='file-import'&&modal?.type==='playlist'){
      const p=playlist(modal.id),files=Array.from(el.files||[]);
      files.forEach((file,i)=>p.tracks.push({id:p.id+'-import-'+Date.now()+'-'+i,title:file.name.replace(/\.[^.]+$/,''),artist:'Добавлено вами',duration:210}));
      el.value='';store();render();toast(`Добавлены названия ${files.length} файлов. Аудио не загружалось на сервер.`);
    }
  });
  document.addEventListener('submit',event=>{
    event.preventDefault();const form=event.target;
    if(form.hasAttribute('data-rule-form'))return saveRule();
    if(form.hasAttribute('data-settings-form')){const data=new FormData(form);if(!String(data.get('name')).trim())return toast('Введите название пространства.');state.settings.name=String(data.get('name')).trim();state.settings.zone=String(data.get('zone'));store();render();return toast('Настройки сохранены.');}
    const data=new FormData(form);
    if(form.id==='new-playlist-form'){const name=String(data.get('name')).trim();if(!name){modalError='Введите название коллекции.';return renderModal();}const id='collection-'+Date.now();state.playlists.push({id,name,description:String(data.get('description')).trim()||'Новая коллекция',tracks:[]});store();closeModal();render();openModal('playlist',{id,limit:10,query:''});return toast('Коллекция создана. Можно добавить файлы.');}
    if(form.id==='exception-form'){const day=String(data.get('date'));if(!day){modalError='Выберите дату.';return renderModal();}if(!ruleDraft.exceptions.includes(day))ruleDraft.exceptions.push(day);ruleDraft.exceptions.sort();markDirty();return renderModal();}
    if(form.id==='template-form'){const kind=dayOfWeek(state.selectedDate)>5?'weekend':'weekday';const rows=state.templates[kind].map((_,i)=>({start:String(data.get('start-'+i)),end:String(data.get('end-'+i)),playlist:String(data.get('playlist-'+i))}));modal.templateDraft=rows;if(rows.some((r,i)=>!r.start||!r.end||r.start>=r.end||i>0&&r.start<rows[i-1].end)){modalError='Проверьте часы: начало раньше окончания, интервалы идут по порядку и не пересекаются.';return renderModal();}state.templates[kind]=rows;resetPreview();store();closeModal();render();return toast('Программа дня сохранена. Предпросмотр обновлён.');}
    if(form.id==='advert-form'){const a={...modal.draft,name:String(data.get('name')).trim(),interval:Number(data.get('interval')),duration:Number(data.get('duration')),start:String(data.get('start')),end:String(data.get('end')),enabled:data.has('enabled')};modal.draft=a;if(!a.name||!a.start||!a.end||a.start>a.end||!Number.isInteger(a.interval)||a.interval<1||a.interval>1440||!Number.isInteger(a.duration)||a.duration<1||a.duration>300){modalError='Проверьте название, даты, периодичность и длительность.';return renderModal();}const index=state.adverts.findIndex(s=>s.id===a.id);if(index<0)state.adverts.push(a);else state.adverts[index]=a;store();closeModal();render();return toast('Рекламная вставка сохранена.');}
  });
  document.getElementById('modal').addEventListener('cancel',()=>{modal=null;modalError='';});
  document.getElementById('modal').addEventListener('keydown',event=>{if(event.key==='Escape'){event.preventDefault();closeModal();}});
  document.getElementById('modal').addEventListener('click',event=>{if(event.target.id==='modal'){const r=event.target.getBoundingClientRect();if(event.clientX<r.left||event.clientX>r.right||event.clientY<r.top||event.clientY>r.bottom)closeModal();}});
  setInterval(()=>{
    if(!state.playback.playing||!state.playback.current)return;
    state.playback.elapsed++;
    if(state.playback.elapsed>=state.playback.current.duration){nextTrack();store();render();return;}
    document.querySelectorAll('[data-elapsed]').forEach(el=>el.textContent=secondsText(state.playback.elapsed));
    document.querySelectorAll('.seek-control').forEach(el=>{if(document.activeElement!==el)el.value=state.playback.elapsed;});
  },1000);
  store();render();
})();
