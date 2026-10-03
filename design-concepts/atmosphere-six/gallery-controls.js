'use strict';
const picker=document.getElementById('comparison-palette');
function applyPalette(id){
  picker.value=id;
  document.querySelectorAll('[data-variant-link]').forEach(a=>a.href=a.dataset.variantLink+'.html'+(id?'?palette='+encodeURIComponent(id):''));
  document.querySelectorAll('[data-palette-choice]').forEach(b=>{b.classList.toggle('active',b.dataset.paletteChoice===id);b.setAttribute('aria-pressed',String(b.dataset.paletteChoice===id))});
  try{localStorage.setItem('mb-tide-gallery-palette',id)}catch{}
}
picker.addEventListener('change',()=>applyPalette(picker.value));
document.querySelectorAll('[data-palette-choice]').forEach(b=>b.addEventListener('click',()=>applyPalette(b.dataset.paletteChoice)));
try{const saved=localStorage.getItem('mb-tide-gallery-palette');if([...picker.options].some(o=>o.value===saved))applyPalette(saved)}catch{}
