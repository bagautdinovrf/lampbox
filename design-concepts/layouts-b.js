(function registerLampBoxLayouts() {
  "use strict";

  const layouts = window.LampBoxLayouts = window.LampBoxLayouts || {};

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
          ${ui.isVideo() ? ui.program() : ui.rotation({ hero: false })}
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
