import assert from 'node:assert/strict';
import {createRequire} from 'node:module';
import {readFile, writeFile, copyFile, mkdir, access, readdir} from 'node:fs/promises';
import {basename, dirname, extname, join, relative} from 'node:path';
import {fileURLToPath} from 'node:url';
import {createHash} from 'node:crypto';
import vm from 'node:vm';

// Reproducible Windows capture. Existing frozen source files are never replaced.
// All browser requests are fulfilled from ./source, and browser storage is isolated.
// Run: node design-concepts/tide-original-reference/capture.mjs
const require = createRequire(import.meta.url);
const {chromium} = require(process.env.PLAYWRIGHT_MODULE || 'C:/Users/bagau/.cache/codex-runtimes/codex-primary-runtime/dependencies/node/node_modules/playwright');
const out = dirname(fileURLToPath(import.meta.url));
const original = join(out, '..', 'atmosphere-six');
const source = join(out, 'source');
const targetUrl = 'http://127.0.0.1:62253/design-concepts/atmosphere-six/tide.html?palette=denim';
let currentPalette = 'denim';
const sha256 = bytes => createHash('sha256').update(bytes).digest('hex');
for (const folder of [source, join(out,'screenshots'), join(out,'icons')]) await mkdir(folder,{recursive:true});
const exists = async path => {try {await access(path); return true;} catch {return false;}};
if (!(await exists(join(source,'tide.html')))) await copyFile(join(original,'tide.html'),join(source,'tide.html'));
const html = await readFile(join(source,'tide.html'),'utf8');
const chain = ['tide.html',...Array.from(html.matchAll(/(?:href|src)="([^"?#]+\.(?:css|js))"/g),match=>match[1])];
for (const name of chain) if (!(await exists(join(source,name)))) await copyFile(join(original,name),join(source,name));
if (!(await exists(join(source,'DESIGN.md')))) await copyFile(join(out,'..','DESIGN.md'),join(source,'DESIGN.md'));
assert(!chain.includes('tide-relief.css'), 'Original must not include Relief styling');
const bytes = new Map(await Promise.all(chain.map(async name=>[name,await readFile(join(source,name))])));
const app = bytes.get('app.js').toString('utf8');
const iconLiteral = app.match(/const paths = (\{[^\r\n]+\});/);
assert(iconLiteral,'Cannot find the literal icon path map in frozen app.js');
const iconPaths = vm.runInNewContext('('+iconLiteral[1]+')',{}, {timeout:1000});
for (const [name,path] of Object.entries(iconPaths)) await writeFile(join(out,'icons',name+'.svg'),`<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round"><path d="${path}"/></svg>\n`);
await writeFile(join(out,'icons','paths.json'),JSON.stringify({source:'source/app.js',viewBox:'0 0 24 24',strokeWidth:1.7,strokeLinecap:'round',strokeLinejoin:'round',paths:iconPaths},null,2));

const sample='АБВГДЕЁЖЗИЙКЛМНОПРСТУФХЦЧШЩЪЫЬЭЮЯабвгдеёжзийклмнопрстуфхцчшщъыьэюя0123456789–—%';
const fonts=[];
async function verifyGlyphs(path) {
  const b=await readFile(path);let cmap;
  for(let i=0;i<b.readUInt16BE(4);i++){const p=12+i*16;if(b.toString('ascii',p,p+4)==='cmap')cmap=b.readUInt32BE(p+8);}
  assert(cmap!==undefined,`No cmap: ${path}`);
  const tables=Array.from({length:b.readUInt16BE(cmap+2)},(_,i)=>cmap+b.readUInt32BE(cmap+8+i*8));
  const has=cp=>tables.some(o=>{
    const format=b.readUInt16BE(o);
    if(format===12)for(let i=0;i<b.readUInt32BE(o+12);i++){const p=o+16+i*12,start=b.readUInt32BE(p),end=b.readUInt32BE(p+4);if(cp>=start&&cp<=end)return b.readUInt32BE(p+8)+cp-start!==0;}
    if(format===4){const n=b.readUInt16BE(o+6)/2,end=o+14,start=end+n*2+2,delta=start+n*2,range=delta+n*2;for(let i=0;i<n;i++){if(cp<b.readUInt16BE(start+i*2)||cp>b.readUInt16BE(end+i*2))continue;const r=b.readUInt16BE(range+i*2),d=b.readInt16BE(delta+i*2);if(!r)return((cp+d)&65535)!==0;const g=b.readUInt16BE(range+i*2+r+2*(cp-b.readUInt16BE(start+i*2)));return g!==0&&((g+d)&65535)!==0;}}
    return false;
  });
  const missing=[...sample].filter(c=>!has(c.codePointAt(0)));
  assert.deepEqual(missing,[]);fonts.push({path,sha256:sha256(b),glyphSample:sample,missing});
}
for(const path of ['C:/Windows/Fonts/segoeui.ttf','C:/Windows/Fonts/seguisb.ttf','C:/Windows/Fonts/segoeuib.ttf'])await verifyGlyphs(path);

const selectors=[
  'body','.app-shell','.app-header','.brand','.brand-mark','.header-context','.header-tools','.variant-control','.variant-picker','.palette-control','.palette-picker','.station-button',
  '.primary-nav','.nav-item','.nav-item.active','.workspace','.page-heading','.page-heading h1','.heading-copy p','.heading-actions','.button','.button.primary','.button.quiet','.button.danger','.icon-button',
  '.ops-overview','.ops-overview > *','.ops-label','.ops-overview strong','.ops-overview p','.ops-context-inputs','#ops-date','#ops-time','.workspace-grid','.main-panel',
  '.feature.ops-selection','.feature h2','.selection-summary','.rule-chips','.rule-chips > span','.feature-action','.channel-panel','.section-heading','.section-heading h3','.channel-list','.channel-card','.channel-card.active','.channel-copy strong','.channel-copy small','.status-badge',
  '.schedule-panel','.schedule-tools','.schedule-toolbar','.day-strip','.day-button','.day-button.active','.timeline-scale','.timeline-row','.timeline-label','.timeline-track','.timeline-span','.timeline-cursor','.schedule-status','.channel-schedule',
  '.data-table','.data-table th','.data-table td','.media-panel','.media-toolbar','.search','.search input','.media-meta','.table-wrap','.media-table','.media-table th','.media-table td','.media-table tr.selected','.table-check','.file-name','.file-name strong','.file-name small','.file-thumb','.type-pill','.drop-hint',
  '.inspector-panel','.inspector-title','.inspector-panel > small','.detail-list','.detail-list dt','.detail-list dd','.inspector-actions','.inspector-rule','.inspector-rule p','.inspector-rule h3','.caption',
  '.transport','.transport-file','.mini-art','.transport-copy strong','.transport-copy small','.transport-controls','.play-button','.stop-button','.transport-state','.transport-actions',
  '.service-view','.service-card','.service-card h2','.service-card p','.form-inline','.form-grid','.field','.field input','.field select','.field output','.format-grid','.check-row','.form-actions','.notice','.empty','.about-brand',
  'dialog[open]','.dialog-body','.dialog-heading','.dialog-heading h2','.dialog-note','.form-error','.weekday-picker','.weekday-picker label','.month-picker','.month-picker label','input[type=checkbox]','input[type=range]','input[type=time]','input[type=date]','input[type=number]','select'
];
const properties=['display','position','box-sizing','grid-template-columns','grid-template-rows','grid-area','grid-column','grid-row','gap','row-gap','column-gap','align-items','align-self','justify-content','flex-direction','flex','width','height','min-width','max-width','min-height','max-height','padding','padding-top','padding-right','padding-bottom','padding-left','margin','margin-top','margin-right','margin-bottom','margin-left','border','border-top','border-right','border-bottom','border-left','border-radius','background-color','background-image','background-size','box-shadow','color','opacity','font-family','font-size','font-weight','font-style','line-height','letter-spacing','text-transform','text-align','white-space','font-variant-numeric','overflow','overflow-x','overflow-y','outline','outline-offset','backdrop-filter','filter','transform','accent-color','appearance','z-index'];
const metrics={reference:'Tide / Original — Denim canonical, Slate/Pine/Berry/Graphite/Pearl additional light themes',targetUrl,lightThemes:['denim','slate','pine','berry','graphite','pearl'],frozenChain:chain,scope:'Fresh isolated demo browser, no real media or station; screenshots preserve the original prototype including its selectors. This pack captures Original; the implementation prompt also supports Relief with independent theme selection. No dark reference is invented.',browser:{},fonts,states:[]};
const labels={reference:metrics.reference,states:[]};
const errors=[];
const browser=await chromium.launch({headless:true,channel:'msedge'});
metrics.browser.version=browser.version();
const context=await browser.newContext({viewport:{width:1440,height:900},deviceScaleFactor:1,reducedMotion:'reduce',locale:'ru-RU',timezoneId:'Europe/Moscow',colorScheme:'light'});
await context.route('**/*',async route=>{
  const name=basename(new URL(route.request().url()).pathname);
  if(bytes.has(name))return route.fulfill({status:200,contentType:extname(name)==='.html'?'text/html; charset=utf-8':extname(name)==='.css'?'text/css; charset=utf-8':'application/javascript; charset=utf-8',body:bytes.get(name)});
  if(name==='favicon.ico')return route.fulfill({status:204,body:''});
  return route.abort();
});
const page=await context.newPage();page.setDefaultTimeout(10000);
page.on('pageerror',error=>errors.push(String(error)));
const ready=async()=>{
  await page.waitForFunction(()=>!!window.MB);
  assert.equal(await page.locator('body').getAttribute('data-variant'),'tide');
  assert.equal(await page.locator('body').getAttribute('data-palette'),currentPalette);
  assert(await page.evaluate(async text=>{await document.fonts.ready;for(const weight of [400,600,700])await document.fonts.load(`${weight} 14px "MediaBoxManager Sans"`,text);return[400,600,700].every(weight=>document.fonts.check(`${weight} 14px "MediaBoxManager Sans"`,text));},sample));
  await page.evaluate(()=>new Promise(resolve=>requestAnimationFrame(()=>requestAnimationFrame(resolve))));
};
const reset=async()=>{await page.goto(targetUrl.replace('palette=denim','palette='+currentPalette));await ready();await page.evaluate(()=>{localStorage.clear();MB.reset();});await ready();};
const nav=async name=>{await page.locator(`[data-page="${name}"]`).click();await page.locator('.workspace').evaluate(el=>el.scrollTo(0,0));await ready();};
const action=async name=>page.locator(`[data-action="${name}"]`).first().click();
const close=async()=>{await page.keyboard.press('Escape');await page.locator('dialog[open]').waitFor({state:'detached'});};
async function capture(id,description){
  await ready();
  const screenshot=`screenshots/${id}.png`;
  await page.screenshot({path:join(out,screenshot),fullPage:false,animations:'disabled'});
  const result=await page.evaluate(({selectors,properties})=>{
    const styleOf=(el,pseudo)=>{const s=getComputedStyle(el,pseudo);return Object.fromEntries(properties.map(p=>[p,s.getPropertyValue(p)]));};
    const viewport={width:innerWidth,height:innerHeight,devicePixelRatio};
    const elements=[];
    for(const selector of selectors){
      [...document.querySelectorAll(selector)].forEach((el,index)=>{
        const r=el.getBoundingClientRect(),style=styleOf(el),visible=style.display!=='none'&&style.visibility!=='hidden'&&r.width>0&&r.height>0;
        if(!visible)return;
        const label=el.getAttribute('aria-label')||el.getAttribute('title')||el.closest('label')?.innerText||'';
        elements.push({selector,index,tag:el.tagName.toLowerCase(),id:el.id,className:typeof el.className==='string'?el.className:'',text:el.innerText||'',label,value:'value'in el?el.value:undefined,checked:'checked'in el?el.checked:undefined,disabled:'disabled'in el?el.disabled:undefined,bbox:{x:r.x,y:r.y,width:r.width,height:r.height,right:r.right,bottom:r.bottom},intersectsViewport:r.right>0&&r.bottom>0&&r.x<innerWidth&&r.y<innerHeight,fullyInsideViewport:r.x>=0&&r.y>=0&&r.right<=innerWidth&&r.bottom<=innerHeight,style,pseudo:el.matches('dialog')?{'::backdrop':styleOf(el,'::backdrop')}:undefined});
      });
    }
    const tokens=Object.fromEntries([...getComputedStyle(document.body)].filter(p=>p.startsWith('--')).map(p=>[p,getComputedStyle(document.body).getPropertyValue(p).trim()]));
    const swatch=document.createElement('span');swatch.style.display='none';document.body.append(swatch);
    const canvas=document.createElement('canvas');canvas.width=canvas.height=1;const paint=canvas.getContext('2d');const resolvedColorTokens={};
    for(const name of ['--bg','--surface','--surface-2','--ink','--muted','--line','--accent','--accent-soft','--accent-line','--accent-ink','--status-ok','--status-ok-bg','--status-warning','--status-warning-bg','--status-error','--status-error-bg']){
      if(!tokens[name])continue;swatch.style.color=`var(${name})`;const css=getComputedStyle(swatch).color;paint.clearRect(0,0,1,1);paint.fillStyle=css;paint.fillRect(0,0,1,1);const rgba=[...paint.getImageData(0,0,1,1).data];resolvedColorTokens[name]={css,hex:'#'+rgba.slice(0,3).map(value=>value.toString(16).padStart(2,'0')).join('')+(rgba[3]===255?'':rgba[3].toString(16).padStart(2,'0')),rgba};
    }swatch.remove();
    const controls=[...document.querySelectorAll('button,a,input,select,label,th,h1,h2,h3')].filter(el=>{const r=el.getBoundingClientRect();return r.width&&r.height&&getComputedStyle(el).visibility!=='hidden';}).map(el=>({tag:el.tagName.toLowerCase(),id:el.id,action:el.getAttribute('data-action'),page:el.getAttribute('data-page'),name:el.getAttribute('name'),text:el.innerText||'',ariaLabel:el.getAttribute('aria-label'),title:el.getAttribute('title'),value:'value'in el?el.value:undefined,options:el.matches('select')?[...el.options].map(option=>({value:option.value,text:option.text,selected:option.selected})):undefined}));
    return{viewport,tokens,resolvedColorTokens,elements,labels:{bodyText:document.body.innerText,dialogText:document.querySelector('dialog[open]')?.innerText||null,controls},demoState:window.MB.getState(),documentExtent:{width:document.documentElement.scrollWidth,height:document.documentElement.scrollHeight}};
  },{selectors,properties});
  labels.states.push({id,theme:currentPalette,description,screenshot,...result.labels});delete result.labels;
  metrics.states.push({id,theme:currentPalette,description,screenshot,...result});
  console.log(`Captured ${id}: ${result.elements.length} metric entries`);
}
try{
  await reset();
  const cdp=await context.newCDPSession(page);await cdp.send('DOM.enable');await cdp.send('CSS.enable');
  const document=await cdp.send('DOM.getDocument');metrics.browser.platformFonts=[];
  for(const selector of ['.page-heading h1','.nav-item.active span','.nav-item:not(.active) span','.file-name strong','.file-name small']){
    const {nodeId}=await cdp.send('DOM.querySelector',{nodeId:document.root.nodeId,selector});
    metrics.browser.platformFonts.push({selector,...await cdp.send('CSS.getPlatformFontsForNode',{nodeId}),cssWeight:await page.locator(selector).first().evaluate(el=>getComputedStyle(el).fontWeight)});
  }
  await capture('01-music-1440x900','Музыка: исходный рабочий экран, 3 октября 2026, 10:24, Оригинал + Деним.');
  await page.setViewportSize({width:1600,height:1000});await capture('02-music-1600x1000','Тот же экран и состояние на большем окне.');await page.setViewportSize({width:1440,height:900});
  await reset();await action('edit-channel');await page.locator('#channel-form').waitFor();await capture('03-channel-editor-1440x900','Оригинал: редактор канала, календарь, громкость.');await close();
  await reset();await nav('settings');await capture('04-settings-1440x900','Оригинал: настройки форматов и визуального комфорта.');
  await reset();await nav('ads');await capture('05-ads-1440x900','Оригинал: рекламные файлы и расписание.');
  for(const [id,palette,name]of [['06-slate-music-1440x900','slate','Сланец'],['07-pine-music-1440x900','pine','Хвоя'],['08-berry-music-1440x900','berry','Ягодная'],['09-graphite-music-1440x900','graphite','Графит'],['10-pearl-music-1440x900','pearl','Жемчуг']]){currentPalette=palette;await reset();await capture(id,name+': оформление Оригинал и исходный музыкальный экран.');}
  assert.deepEqual(errors,[]);
} finally {await context.close();await browser.close();}
metrics.pageErrors=errors;metrics.capturedAt=new Date().toISOString();
await writeFile(join(out,'computed-metrics.json'),JSON.stringify(metrics,null,2));
await writeFile(join(out,'visible-labels.json'),JSON.stringify(labels,null,2));
await writeFile(join(out,'theme-tokens.json'),JSON.stringify({note:'Measured browser colors. Hex values are sRGB canvas conversions of computed CSS colors. Dark is a separate proposed target, not a captured original.',themes:Object.fromEntries(metrics.lightThemes.map(theme=>{const state=metrics.states.find(item=>item.theme===theme);return[theme,{sourceState:state.id,tokens:state.tokens,colors:state.resolvedColorTokens}];}))},null,2));
const artifactFiles=['capture.mjs','computed-metrics.json','visible-labels.json','theme-tokens.json',...chain.map(name=>'source/'+name),'source/DESIGN.md',...Object.keys(iconPaths).map(name=>'icons/'+name+'.svg'),'icons/paths.json',...metrics.states.map(state=>state.screenshot)];
const manifest={schemaVersion:1,reference:metrics.reference,targetUrl,capturedAt:metrics.capturedAt,browser:metrics.browser,lightThemes:metrics.lightThemes,viewports:[{width:1440,height:900},{width:1600,height:1000}],sourceFiles:chain.map(name=>({path:'source/'+name,original:'../atmosphere-six/'+name})),additionalGuidance:'source/DESIGN.md',notes:['Six selected light palettes are captured; no dark UI existed in the source.','Source files are byte-for-byte snapshots and are served to an isolated browser using request interception.','Palette/variant selectors in the original prototype are preserved in the source and screenshots; the implementation prompt specifies Original and Relief with independently selected themes.','Screenshot pixels are CSS pixels at deviceScaleFactor=1. Font files were verified locally but are not redistributed.','Native input decorations and glyph rasterization may differ across browser/Qt/OS backends.'],files:[]};
for(const name of artifactFiles){const content=await readFile(join(out,name));manifest.files.push({path:name.replaceAll('\\','/'),bytes:content.length,sha256:sha256(content)});}
await writeFile(join(out,'manifest.json'),JSON.stringify(manifest,null,2));
console.log(`Completed ${metrics.states.length} states, ${chain.length} frozen loaded sources, ${Object.keys(iconPaths).length} SVG icons, ${manifest.files.length} hashed artifacts.`);
