(function registerLampBoxLayouts() {
  "use strict";

  const layouts = window.LampBoxLayouts = window.LampBoxLayouts || {};

  layouts.silver = function silverLayout(ui) {
    return `
      <style>
        .silver-workspace {
          display: grid;
          grid-template-columns: minmax(0, .82fr) minmax(0, 1.5fr) minmax(0, 1fr);
          gap: 22px;
          align-items: start;
          width: 100%;
          min-width: 0;
        }
        .silver-rules,
        .silver-editor,
        .silver-live {
          display: grid;
          grid-template-columns: minmax(0, 1fr);
          gap: 20px;
          align-content: start;
          min-width: 0;
        }
        .silver-bottom {
          grid-column: 1 / -1;
          min-width: 0;
        }
        .silver-workspace > *,
        .silver-rules > *,
        .silver-editor > *,
        .silver-live > * {
          min-width: 0;
          max-width: 100%;
        }
        @media (min-width: 1500px) {
          .silver-workspace { gap: 28px; }
          .silver-rules,
          .silver-editor,
          .silver-live { gap: 24px; }
        }
        @media (max-width: 1250px) {
          .silver-workspace {
            grid-template-columns: minmax(0, 1.45fr) minmax(0, 1fr);
            gap: 20px;
          }
          .silver-rules {
            grid-column: 1 / -1;
            grid-template-columns: minmax(0, .92fr) minmax(0, 1.08fr);
          }
          .silver-editor { grid-column: 1; }
          .silver-live { grid-column: 2; }
        }
        @media (max-width: 850px) {
          .silver-workspace {
            grid-template-columns: minmax(0, 1fr);
            gap: 20px;
          }
          .silver-editor,
          .silver-live { grid-column: 1; }
          .silver-live {
            grid-template-columns: minmax(0, 1fr) minmax(0, 1fr);
          }
        }
        @media (max-width: 600px) {
          .silver-workspace,
          .silver-rules,
          .silver-editor,
          .silver-live {
            grid-template-columns: minmax(0, 1fr);
            gap: 16px;
          }
        }
      </style>
      <div class="silver-workspace">
        <div class="silver-rules">
          ${ui.ruleList()}
          ${ui.calendar({ compact: true })}
        </div>
        <div class="silver-editor">
          ${ui.ruleEditor()}
          ${ui.rotation({ hero: false })}
        </div>
        <div class="silver-live">
          ${ui.player({ compact: true })}
          ${ui.queue({ strip: false })}
        </div>
        <div class="silver-bottom">
          ${ui.program()}
        </div>
      </div>
    `;
  };

  layouts.library = function libraryLayout(ui) {
    return `
      <style>
        .library-workspace {
          display: grid;
          grid-template-columns: minmax(0, 1fr) minmax(0, 300px);
          gap: 26px;
          align-items: start;
          width: 100%;
          min-width: 0;
        }
        .library-collection {
          min-width: 0;
        }
        .library-rule {
          display: grid;
          grid-template-columns: minmax(0, 1fr);
          align-content: start;
          gap: 20px;
          min-width: 0;
        }
        .library-footer {
          grid-column: 1 / -1;
          min-width: 0;
        }
        .library-workspace > *,
        .library-rule > * {
          min-width: 0;
          max-width: 100%;
        }
        @media (min-width: 1500px) {
          .library-workspace {
            grid-template-columns: minmax(0, 1fr) minmax(0, 320px);
            gap: 32px;
          }
          .library-rule { gap: 24px; }
        }
        @media (max-width: 1150px) {
          .library-workspace {
            grid-template-columns: minmax(0, 1fr) minmax(0, 270px);
            gap: 20px;
          }
          .library-rule { gap: 18px; }
        }
        @media (max-width: 850px) {
          .library-workspace {
            grid-template-columns: minmax(0, 1fr);
            gap: 20px;
          }
          .library-rule {
            grid-template-columns: minmax(0, 1fr) minmax(0, 1fr);
            align-items: start;
          }
          .library-rule > :last-child { grid-column: 1 / -1; }
        }
        @media (max-width: 600px) {
          .library-workspace,
          .library-rule {
            grid-template-columns: minmax(0, 1fr);
            gap: 16px;
          }
        }
      </style>
      <div class="library-workspace">
        <div class="library-collection">
          ${ui.library({ compact: false })}
        </div>
        <div class="library-rule">
          ${ui.rotation({ hero: false })}
          ${ui.calendar({ compact: true })}
          ${ui.player({ compact: true })}
        </div>
        <div class="library-footer">
          ${ui.queue({ strip: true })}
        </div>
      </div>
    `;
  };

  layouts.timeline = function timelineLayout(ui) {
    return `
      <style>
        .timeline-workspace {
          display: grid;
          grid-template-columns: minmax(0, 1fr) minmax(0, 286px);
          gap: 28px;
          align-items: start;
          width: 100%;
          min-width: 0;
        }
        .timeline-main {
          min-width: 0;
        }
        .timeline-live {
          display: grid;
          grid-template-columns: minmax(0, 1fr);
          align-content: start;
          gap: 20px;
          min-width: 0;
        }
        .timeline-week {
          grid-column: 1 / -1;
          min-width: 0;
        }
        .timeline-workspace > *,
        .timeline-live > * {
          min-width: 0;
          max-width: 100%;
        }
        @media (min-width: 1500px) {
          .timeline-workspace {
            grid-template-columns: minmax(0, 1fr) minmax(0, 310px);
            gap: 34px;
          }
          .timeline-live { gap: 24px; }
        }
        @media (max-width: 1150px) {
          .timeline-workspace {
            grid-template-columns: minmax(0, 1fr) minmax(0, 260px);
            gap: 22px;
          }
          .timeline-live { gap: 18px; }
        }
        @media (max-width: 850px) {
          .timeline-workspace {
            grid-template-columns: minmax(0, 1fr);
            gap: 20px;
          }
          .timeline-live {
            grid-template-columns: minmax(0, 1fr) minmax(0, 1fr);
            align-items: start;
          }
        }
        @media (max-width: 600px) {
          .timeline-workspace,
          .timeline-live {
            grid-template-columns: minmax(0, 1fr);
            gap: 16px;
          }
        }
      </style>
      <div class="timeline-workspace">
        <div class="timeline-main">
          ${ui.timeline()}
        </div>
        <div class="timeline-live">
          ${ui.player({ compact: false })}
          ${ui.queue({ strip: false })}
        </div>
        <div class="timeline-week">
          ${ui.week()}
        </div>
      </div>
    `;
  };
}());
