"""Offline conversion of pinned service artwork. Requires Pillow and CairoSVG."""
from pathlib import Path
import hashlib
import io
import json
import xml.etree.ElementTree as ET
import cairosvg
from PIL import Image

root = Path(__file__).resolve().parent.parent
assets = root / "resources/services"
colors = {"youtube": "FF0000", "chatgpt": "171717", "github": "171717", "x": "171717",
          "netflix": "E50914", "tiktok": "171717", "facebook": "0866FF", "instagram": "D83780",
          "telegram": "26A5E4", "cloudflare": "F38020"}
slugs = {"chatgpt": "openai"}
entries = []
for name in ["youtube", "chatgpt", "google", "github", "x", "netflix", "disney", "tiktok",
             "facebook", "instagram", "telegram", "cloudflare"]:
    source = assets / "source" / (name + (".png" if name == "google" else ".ico" if name == "disney" else ".svg"))
    raw = source.read_bytes()
    if name in colors:
        svg = ET.fromstring(raw)
        svg.set("fill", "#" + colors[name])
        png = cairosvg.svg2png(bytestring=ET.tostring(svg), output_width=96, output_height=96)
        image = Image.open(io.BytesIO(png)).convert("RGBA")
        if name == "instagram":
            gradient = Image.new("RGBA", image.size)
            for y in range(96):
                for x in range(96):
                    t = (y + (95 - x) * 0.25) / 120
                    gradient.putpixel((x, y), (int(145 + 100*t), int(47 + 89*t), int(210 - 151*t), 255))
            gradient.putalpha(image.getchannel("A")); image = gradient
        if name == "tiktok":
            cyan = Image.new("RGBA", image.size, "#25F4EE"); cyan.putalpha(image.getchannel("A"))
            pink = Image.new("RGBA", image.size, "#FE2C55"); pink.putalpha(image.getchannel("A"))
            composite = Image.new("RGBA", image.size)
            composite.alpha_composite(cyan, (-2, -2)); composite.alpha_composite(pink, (2, 2)); composite.alpha_composite(image)
            image = composite
        url = "https://raw.githubusercontent.com/simple-icons/simple-icons/13.21.0/icons/" + slugs.get(name, name) + ".svg"
        license_name = "CC0-1.0"
    else:
        image = Image.open(source).convert("RGBA").resize((96, 96), Image.Resampling.LANCZOS)
        url = "https://www.disneyplus.com/favicon.ico" if name == "disney" else "https://www.gstatic.com/images/branding/product/1x/googleg_48dp.png"
        license_name = "Respective brand owner; destination identifier"
    image.save(assets / (name + ".png"), optimize=True)
    entries.append(dict(id=name, url=url, source=str(source.relative_to(assets)).replace("\\", "/"),
                        source_sha256=hashlib.sha256(raw).hexdigest(), license=license_name,
                        sha256=hashlib.sha256((assets / (name + ".png")).read_bytes()).hexdigest()))
(assets / "manifest.json").write_text(json.dumps(dict(files=entries), ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
(root / "resources/services.qrc").write_text('<RCC><qresource prefix="/services">' + ''.join(
    f'<file alias="{e["id"]}.png">services/{e["id"]}.png</file>' for e in entries) + '</qresource></RCC>\n', encoding="utf-8")
