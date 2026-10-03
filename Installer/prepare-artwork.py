"""Export the reviewed emblem to the native Inno Setup image formats."""

from pathlib import Path

from PIL import Image, ImageOps


ASSETS = Path(__file__).resolve().parent / "assets"
emblem = Image.open(ASSETS / "emblem.png").convert("RGBA")
content_bounds = emblem.getchannel("A").getbbox()
if not content_bounds:
    raise ValueError("The installer emblem is empty")
emblem = emblem.crop(content_bounds)


def place_emblem(size, max_size, background, center):
    canvas = Image.new("RGBA", size, background)
    mark = ImageOps.contain(emblem, max_size, Image.Resampling.LANCZOS)
    position = (round(center[0] - mark.width / 2), round(center[1] - mark.height / 2))
    canvas.alpha_composite(mark, position)
    return canvas


icon = place_emblem((256, 256), (218, 218), (0, 0, 0, 0), (128, 128))
icon.save(ASSETS / "setup.ico", sizes=[(n, n) for n in (16, 20, 24, 32, 40, 48, 64, 128, 256)])
icon.save(ASSETS.parent.parent / "src" / "icons" / "app.ico",
          sizes=[(n, n) for n in (16, 20, 24, 32, 40, 48, 64, 128, 256)])

# Three times the standard dimensions keep the artwork crisp on high-DPI displays.
large = place_emblem((492, 942), (360, 360), "#F5F5F7", (246, 405))
large.convert("RGB").save(ASSETS / "wizard-large.bmp")
small = place_emblem((165, 165), (135, 135), "white", (82.5, 82.5))
small.convert("RGB").save(ASSETS / "wizard-small.bmp")
preview = place_emblem((384, 384), (280, 280), "white", (192, 192))
preview.convert("RGB").save(ASSETS / "emblem-preview.png")
print("Exported installer and application icons, wizard bitmaps and emblem-preview.png")
