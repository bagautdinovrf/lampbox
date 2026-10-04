import fs from 'node:fs';
import path from 'node:path';
import {fileURLToPath} from 'node:url';
import {createRequire} from 'node:module';
const require=createRequire(import.meta.url);
const {chromium}=require('C:/Users/bagau/.cache/codex-runtimes/codex-primary-runtime/dependencies/node/node_modules/playwright');
const dir=path.dirname(fileURLToPath(import.meta.url));
const designs=['tide','tide-relief','tide-glass','tide-signal','tide-console','tide-satin'];
const chars=[...new Set('АБВГДЕЁЖЗИЙКЛМНОПРСТУФХЦЧШЩЪЫЬЭЮЯабвгдеёжзийклмнопрстуфхцчшщъыьэюя0123456789—–%')];
function glyphs(font){const b=fs.readFileSync(font);let cmap;for(let i=0;i<b.readUInt16BE(4);i++){let p=12+i*16;if(b.toString('ascii',p,p+4)==='cmap')cmap=b.readUInt32BE(p+8)}if(cmap==null)throw Error('No cmap');const subtables=[];for(let i=0;i<b.readUInt16BE(cmap+2);i++){const p=cmap+4+i*8;subtables.push(cmap+b.readUInt32BE(p+4))}function has(cp){for(const o of subtables){const format=b.readUInt16BE(o);if(format===12){for(let i=0;i<b.readUInt32BE(o+12);i++){const p=o+16+i*12,start=b.readUInt32BE(p),end=b.readUInt32BE(p+4);if(cp>=start&&cp<=end)return b.readUInt32BE(p+8)+cp-start!==0}}if(format===4){const n=b.readUInt16BE(o+6)/2,end=o+14,start=end+n*2+2,delta=start+n*2,range=delta+n*2;for(let i=0;i<n;i++){if(cp<b.readUInt16BE(start+i*2)||cp>b.readUInt16BE(end+i*2))continue;const r=b.readUInt16BE(range+i*2),d=b.readInt16BE(delta+i*2);if(!r)return ((cp+d)&65535)!==0;const loc=range+i*2+r+2*(cp-b.readUInt16BE(start+i*2));const g=b.readUInt16BE(loc);return g!==0&&((g+d)&65535)!==0}}}return false}return {file:font,missing:chars.filter(c=>!has(c.codePointAt(0)))}}
const fontResults=['C:/Windows/Fonts/segoeui.ttf','C:/Windows/Fonts/segoeuib.ttf','C:/Windows/Fonts/seguisb.ttf','C:/Windows/Fonts/georgia.ttf'].map(glyphs);
if(fontResults.some(f=>f.missing.length))throw Error(JSON.stringify(fontResults));
console.log('Font cmap coverage verified: Cyrillic including Ё/Й; all four fonts.');
const browser=await chromium.launch({headless:true,channel:'msedge'});
const page=await browser.newPage({viewport:{width:1440,height:900},deviceScaleFactor:1});
const errors=[];page.on('pageerror',e=>errors.push(e.message));
const result={date:'2026-10-03',fonts:fontResults,designs:[],errors};
for(const design of designs){await page.goto('http://127.0.0.1:4176/'+design+'.html');await page.evaluate(()=>{localStorage.clear();});await page.reload();await page.evaluate(()=>document.fonts.ready);const loaded=await page.evaluate(async()=>{await document.fonts.load('14px "MediaBoxManager Sans"','Музыка Ёж');return document.fonts.check('14px "MediaBoxManager Sans"','Музыка Ёж')});if(!loaded)throw Error('Font failed '+design);
const sizes=await page.evaluate(()=>({viewport:innerWidth,scroll:document.documentElement.scrollWidth,height:document.documentElement.scrollHeight,variant:document.body.dataset.variant}));
if(sizes.variant!==design)throw Error('Wrong variant: '+sizes.variant+' expected '+design);
await page.screenshot({path:path.join(dir,'previews',design+'.png'),fullPage:true});
result.designs.push({design,loaded,sizes});console.log(design,JSON.stringify(sizes));}
for(const doc of ['index','coverage','sources']){await page.goto('http://127.0.0.1:4176/'+doc+'.html');await page.evaluate(()=>document.fonts.ready);await page.screenshot({path:path.join(dir,'previews',doc+'.png'),fullPage:true});}
fs.writeFileSync(path.join(dir,'verification.json'),JSON.stringify(result,null,2));await browser.close();
