(function () {
  'use strict';

  const layouts = window.LampBoxLayouts = window.LampBoxLayouts || {};

  layouts.air = function (ui) {
    return `
      <style>
        .air-workspace{display:grid;grid-template-columns:minmax(0,1.72fr) minmax(300px,1fr);gap:22px;align-items:start}
        .air-main,.air-monitor{display:grid;gap:22px;min-width:0}
        .air-day-grid{display:grid;grid-template-columns:minmax(0,.9fr) minmax(0,1.12fr);gap:22px;align-items:start;min-width:0}
        .air-workspace>*,.air-main>*,.air-monitor>*,.air-day-grid>*{min-width:0}
        .air-main>.panel{margin:0}
        @media(max-width:1200px){.air-workspace{grid-template-columns:minmax(0,1.34fr) minmax(290px,1fr);gap:18px}.air-main,.air-monitor{gap:18px}.air-day-grid{grid-template-columns:minmax(0,1fr);gap:18px}}
        @media(max-width:900px){.air-workspace{grid-template-columns:minmax(0,1fr)}.air-day-grid{grid-template-columns:repeat(2,minmax(0,1fr))}.air-monitor{grid-template-columns:repeat(2,minmax(0,1fr));align-items:start}}
        @media(max-width:640px){.air-workspace,.air-main,.air-monitor,.air-day-grid{gap:16px}.air-day-grid,.air-monitor{grid-template-columns:minmax(0,1fr)}}
      </style>
      <div class="air-workspace">
        <div class="air-main">
          ${ui.week()}
          <div class="air-day-grid">
            ${ui.program()}
            ${ui.rotation({ hero: false })}
          </div>
        </div>
        <aside class="air-monitor" aria-label="Воспроизведение и очередь">
          ${ui.player({ compact: false })}
          ${ui.queue({ strip: false })}
        </aside>
      </div>`;
  };

  layouts.studio = function (ui) {
    return `
      <style>
        .studio-workspace{display:grid;gap:24px;min-width:0}
        .studio-stage{display:grid;grid-template-columns:minmax(0,1.45fr) minmax(320px,1fr);gap:24px;align-items:start;min-width:0}
        .studio-feature,.studio-monitor{display:grid;gap:20px;min-width:0}
        .studio-workspace>*,.studio-stage>*,.studio-feature>*,.studio-monitor>*,.studio-week>*{min-width:0}
        .studio-week{min-width:0}
        @media(max-width:1100px){.studio-workspace,.studio-stage{gap:20px}.studio-stage{grid-template-columns:minmax(0,1.2fr) minmax(290px,1fr)}.studio-feature,.studio-monitor{gap:18px}}
        @media(max-width:800px){.studio-stage{grid-template-columns:minmax(0,1fr)}.studio-monitor{grid-template-columns:repeat(2,minmax(0,1fr));align-items:start}.studio-feature{grid-template-columns:minmax(0,1fr)}}
        @media(max-width:600px){.studio-workspace,.studio-stage,.studio-feature,.studio-monitor{gap:16px}.studio-monitor{grid-template-columns:minmax(0,1fr)}}
      </style>
      <div class="studio-workspace">
        <div class="studio-stage">
          <div class="studio-feature">
            ${ui.rotation({ hero: true })}
            ${ui.program()}
          </div>
          <aside class="studio-monitor" aria-label="Студийный монитор">
            ${ui.player({ compact: false })}
            ${ui.queue({ strip: false })}
          </aside>
        </div>
        <div class="studio-week">
          ${ui.week()}
        </div>
      </div>`;
  };

  layouts.canvas = function (ui) {
    return `
      <style>
        .canvas-workspace{display:grid;grid-template-columns:minmax(0,1.75fr) minmax(290px,1fr);gap:22px;align-items:start;min-width:0}
        .canvas-calendar,.canvas-inspector{min-width:0}
        .canvas-inspector{display:grid;gap:20px}
        .canvas-workspace>*,.canvas-calendar>*,.canvas-inspector>*,.canvas-dock>*{min-width:0}
        .canvas-dock{position:sticky;bottom:18px;z-index:3;margin-top:22px;min-width:0}
        @media(max-width:1200px){.canvas-workspace{grid-template-columns:minmax(0,1.45fr) minmax(280px,1fr);gap:18px}.canvas-inspector{gap:18px}.canvas-dock{margin-top:18px}}
        @media(max-width:900px){.canvas-workspace{grid-template-columns:minmax(0,1fr)}.canvas-inspector{grid-template-columns:repeat(2,minmax(0,1fr));align-items:start}.canvas-dock{position:static}}
        @media(max-width:640px){.canvas-workspace,.canvas-inspector{gap:16px}.canvas-inspector{grid-template-columns:minmax(0,1fr)}.canvas-dock{margin-top:16px}}
      </style>
      <div class="canvas-workspace">
        <div class="canvas-calendar">
          ${ui.calendar({ compact: false })}
        </div>
        <aside class="canvas-inspector" aria-label="Правила выбранного дня">
          ${ui.inspector()}
          ${ui.rotation({ hero: false })}
        </aside>
      </div>
      <div class="canvas-dock">
        ${ui.player({ compact: true })}
      </div>`;
  };

  layouts.focus = function (ui) {
    return `
      <style>
        .focus-workspace{display:grid;grid-template-columns:minmax(0,1.55fr) minmax(300px,1fr);gap:22px;align-items:start;min-width:0}
        .focus-editor,.focus-context{min-width:0}
        .focus-context{display:grid;gap:20px}
        .focus-workspace>*,.focus-editor>*,.focus-context>*,.focus-sequence>*,.focus-player>*{min-width:0}
        .focus-sequence{margin-top:22px;min-width:0}
        .focus-player{margin-top:18px;min-width:0}
        @media(max-width:1100px){.focus-workspace{grid-template-columns:minmax(0,1.38fr) minmax(290px,1fr);gap:18px}.focus-context{gap:18px}.focus-sequence{margin-top:18px}}
        @media(max-width:900px){.focus-workspace{grid-template-columns:minmax(0,1fr)}.focus-context{grid-template-columns:repeat(2,minmax(0,1fr));align-items:start}}
        @media(max-width:640px){.focus-workspace,.focus-context{gap:16px}.focus-context{grid-template-columns:minmax(0,1fr)}.focus-sequence,.focus-player{margin-top:16px}}
      </style>
      <div class="focus-workspace">
        <div class="focus-editor">
          ${ui.ruleEditor()}
        </div>
        <aside class="focus-context" aria-label="Календарь и основная программа">
          ${ui.calendar({ compact: true })}
          ${ui.program()}
        </aside>
      </div>
      <div class="focus-sequence">
        ${ui.queue({ strip: true })}
      </div>
      <div class="focus-player">
        ${ui.player({ compact: true })}
      </div>`;
  };
}());
