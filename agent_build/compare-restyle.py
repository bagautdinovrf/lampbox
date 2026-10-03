#!/usr/bin/env python3
"""Compare captured Qt windows with the frozen references, without masking panels.

Usage: python agent_build/compare-restyle.py agent_build/restyle-review
Requires Pillow. Inputs are produced by MediaBoxManagerRestyleTests --capture.
The dark theme has no approved reference and is deliberately not diffed.
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path

from PIL import Image, ImageChops, ImageDraw, ImageEnhance, ImageStat


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("captures", type=Path)
    args = parser.parse_args()
    repo = Path(__file__).resolve().parents[1]
    output = args.captures.resolve()
    comparison = output / "comparisons"
    comparison.mkdir(parents=True, exist_ok=True)
    captured = json.loads((output / "capture.json").read_text())
    by_id = {entry["id"]: entry for entry in captured["captures"]}
    results = []
    skipped = []
    for appearance, reference_name in (
        ("tide-relief", "relief-denim-reference"),
        ("tide", "tide-original-reference"),
    ):
        reference_dir = repo / "design-concepts" / reference_name
        reference = json.loads((reference_dir / "computed-metrics.json").read_text())
        for state in reference["states"]:
            state_id = state["id"]
            page = next((name for name in (
                "channel-editor", "all-schedules", "file-info", "delete-confirm",
                "ad-exact", "ad-frequency", "plan-details", "settings", "about",
                "music", "video", "ads", "report", "station",
            ) if name in state_id), None)
            if not page:
                continue
            if page == "delete-confirm":
                skipped.append({"reference": f"{appearance}/{state_id}",
                                "reason": "The Qt capture confirms file deletion; the frozen reference confirms channel deletion. These are different actions, so an overlay is not an equivalent-state comparison."})
                continue
            page = {"ads": "advert", "report": "reports"}.get(page, page)
            viewport = state["viewport"]
            capture_id = f"{appearance}-{state['theme']}-{page}-{viewport['width']}x{viewport['height']}"
            actual_file = output / f"{capture_id}.png"
            if capture_id not in by_id or not actual_file.is_file():
                continue
            expected = Image.open(reference_dir / state["screenshot"]).convert("RGB")
            actual = Image.open(actual_file).convert("RGB")
            if actual.size != expected.size:
                results.append({"capture": capture_id, "reference": state_id,
                                "error": "Different pixel dimensions; no resizing or misleading overlay applied"})
                continue
            difference = ImageChops.difference(expected, actual)
            Image.blend(expected, actual, 0.5).save(comparison / f"{capture_id}-overlay.png")
            ImageEnhance.Contrast(difference).enhance(3).save(comparison / f"{capture_id}-diff.png")
            regions = []
            for element in state["elements"]:
                if element["index"] != 0 or element["selector"] not in {
                    ".app-header", ".primary-nav", ".page-heading", ".ops-overview", ".channel-panel",
                    ".inspector-panel", ".schedule-panel", ".media-panel", ".media-table",
                    ".transport", ".dialog",
                }:
                    continue
                box = element["bbox"]
                bounds = (max(0, round(box["x"])), max(0, round(box["y"])),
                          min(actual.width, round(box["right"])), min(actual.height, round(box["bottom"])))
                if bounds[2] <= bounds[0] or bounds[3] <= bounds[1]:
                    continue
                stats = ImageStat.Stat(difference.crop(bounds))
                regions.append({"selector": element["selector"], "bbox": bounds,
                                "meanAbsoluteRgbDifference": [round(value, 3) for value in stats.mean]})
            geometry = []
            objects = {".app-header": "appHeader", ".primary-nav": "navigation",
                       ".ops-overview": "planSummary", ".feature.ops-selection": "selectedChannel",
                       ".channel-panel": "channelPanel", ".schedule-panel": "schedulePanel",
                       ".media-panel": "libraryPanel", ".inspector-panel": "fileInspector",
                       ".transport": "transport"}
            for element in state["elements"]:
                object_name = objects.get(element["selector"])
                actual_rect = by_id[capture_id]["geometry"].get(object_name)
                if element["index"] != 0 or not actual_rect:
                    continue
                box = element["bbox"]
                expected_rect = [box[key] for key in ("x", "y", "width", "height")]
                geometry.append({"selector": element["selector"], "qtObject": object_name,
                                 "expected": expected_rect, "actual": actual_rect,
                                 "delta": [round(actual - expected, 3)
                                           for actual, expected in zip(actual_rect, expected_rect)]})
            results.append({"capture": capture_id, "reference": state_id,
                            "regions": regions, "geometry": geometry})

    main_images = [entry for entry in captured["captures"]
                   if entry["id"].endswith("music-1440x900")]
    columns, thumb_width, thumb_height, caption = 4, 360, 225, 25
    sheet = Image.new("RGB", (columns * thumb_width,
                              ((len(main_images) + columns - 1) // columns) * (thumb_height + caption)), "#e8ecf0")
    draw = ImageDraw.Draw(sheet)
    for number, entry in enumerate(main_images):
        x, y = (number % columns) * thumb_width, (number // columns) * (thumb_height + caption)
        picture = Image.open(output / f"{entry['id']}.png").convert("RGB")
        picture.thumbnail((thumb_width, thumb_height))
        sheet.paste(picture, (x, y + caption))
        draw.text((x + 8, y + 6), entry["id"].removesuffix("-music-1440x900"), fill="#25303c")
    if main_images:
        sheet.save(output / "all-14-combinations.png")
    report = {
        "fontFamily": captured["fontFamily"],
        "comparisons": results,
        "skipped": skipped,
        "interpretation": "Unmasked visual differences, not a pass/fail score. Production text and underscore names differ from browser fixtures; Qt and browser glyph rasterization differ. The reference library/inspector extend below the viewport; Qt confines those surfaces and scrolls their contents to preserve access. Assess individual panels; no global similarity percentage is reported.",
        "unsupportedValidation": "The Linux offscreen renderer cannot verify physical Windows desktop/taskbar geometry or Segoe UI installed on Windows. Dark captures have no approved browser reference.",
    }
    (comparison / "report.json").write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n")
    print(f"Wrote {len(results)} unmasked comparisons to {comparison}")


if __name__ == "__main__":
    main()
