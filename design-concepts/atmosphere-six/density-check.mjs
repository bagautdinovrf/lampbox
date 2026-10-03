import fs from 'node:fs';
import path from 'node:path';
import {fileURLToPath} from 'node:url';
import {createRequire} from 'node:module';
const require=createRequire(import.meta.url);
const {chromium}=require('C:/Users/bagau/.cache/codex-runtimes/codex-primary-runtime/dependencies/node/node_modules/playwright');
const dir=path.dirname(fileURLToPath(import.meta.url));
const browser=await chromium.launch({headless:true,channel:'msedge'});
const page=await browser.newPage({viewport:{width:1440,height:900}});
const results=[];
for(const design of ['tide','tide-relief','tide-glass','tide-signal','tide-console','tide-satin']) {
  await page.goto('http://127.0.0.1:4176/'+design+'.html');
  await page.evaluate(()=>{localStorage.clear()});await page.reload();
  const variant=await page.locator('body').getAttribute('data-variant');
  if(variant!==design)throw Error('Wrong variant: '+variant+' expected '+design);
  for(const section of ['music','video','ads']) {
    await page.locator('[data-page="'+section+'"]').click();
    const result=await page.evaluate(()=>{
      const work=document.querySelector('.workspace').getBoundingClientRect();
      const footer=document.querySelector('.transport').getBoundingClientRect();
      const rows=[...document.querySelectorAll('.media-table tbody tr')];
      const visible=rows.filter(e=>{const r=e.getBoundingClientRect();return r.height>0&&r.top>=work.top&&r.bottom<=footer.top});
      const panels=['.channel-panel','.feature','.schedule-panel','.ad-schedule','.media-panel','.inspector-panel'].map(selector=>{const el=document.querySelector(selector);if(!el||!el.checkVisibility())return null;const r=el.getBoundingClientRect();return {selector,x:r.x,y:r.y,right:r.right,bottom:r.bottom,width:r.width,height:r.height}}).filter(Boolean);
      const overlaps=[];for(let i=0;i<panels.length;i++)for(let j=i+1;j<panels.length;j++){const a=panels[i],b=panels[j];if(Math.min(a.right,b.right)-Math.max(a.x,b.x)>2&&Math.min(a.bottom,b.bottom)-Math.max(a.y,b.y)>2)overlaps.push([a.selector,b.selector])}
      return {visibleMediaRows:visible.length,totalMediaRows:rows.length,requiredRows:Math.min(3,rows.length),overlaps,panels,horizontalOverflow:document.documentElement.scrollWidth>innerWidth+1};
    });
    result.pass=result.visibleMediaRows>=result.requiredRows&&!result.overlaps.length&&!result.horizontalOverflow;
    results.push({design,section,...result});
    console.log(design,section,'visible rows',result.visibleMediaRows,'overlaps',JSON.stringify(result.overlaps),result.pass?'PASS':'FAIL');
  }
}
fs.writeFileSync(path.join(dir,'density-results.json'),JSON.stringify({viewport:{width:1440,height:900},results},null,2));
await browser.close();
console.log('Passed',results.filter(r=>r.pass).length,'of',results.length);
process.exitCode=results.some(r=>!r.pass)?1:0;
