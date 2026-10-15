"""Generate a Windows icon/resource without stretching the source artwork."""
import argparse
from pathlib import Path
from PIL import Image, ImageOps

parser = argparse.ArgumentParser()
parser.add_argument("source", type=Path)
parser.add_argument("output", type=Path)
args = parser.parse_args()
args.output.mkdir(parents=True, exist_ok=True)
with Image.open(args.source) as source:
    art = ImageOps.contain(source.convert("RGBA"), (256, 256), Image.Resampling.LANCZOS)
canvas = Image.new("RGBA", (256, 256), (0, 0, 0, 0))
canvas.alpha_composite(art, ((256 - art.width) // 2, (256 - art.height) // 2))
icon = args.output / "icon.ico"
canvas.save(icon, sizes=[(n, n) for n in (16, 20, 24, 32, 40, 48, 64, 128, 256)])
(args.output / "icon.rc").write_text(f'1 ICON "{icon.resolve().as_posix()}"\n', encoding="utf-8")
