"""Generate the brand and static tray variants from one SVG master. Requires Pillow."""
from pathlib import Path
from copy import deepcopy
import xml.etree.ElementTree as ET
from PIL import Image, ImageDraw, ImageFont

root = Path(__file__).resolve().parent.parent / "resources"
namespace = "http://www.w3.org/2000/svg"
ET.register_namespace("", namespace)
master = ET.parse(root / "mlash.svg").getroot()
scale = 16


def node(kind, **attrs):
    return ET.Element("{" + namespace + "}" + kind, {key.replace("_", "-"): str(value) for key, value in attrs.items()})


def line(points, color, width=2):
    return node("polyline", points=points, fill="none", stroke=color, stroke_width=width, stroke_linecap="round", stroke_linejoin="round")


def render(svg):
    image = Image.new("RGBA", (64*scale, 64*scale))
    draw = ImageDraw.Draw(image)
    for item in svg:
        kind, attrs = item.tag.rsplit("}", 1)[-1], item.attrib
        fill = attrs.get("fill")
        fill = None if fill == "none" else fill
        if kind == "rect":
            x, y, w, h = [float(attrs[key]) for key in ("x", "y", "width", "height")]
            draw.rounded_rectangle((x*scale, y*scale, (x+w)*scale, (y+h)*scale), float(attrs["rx"])*scale, fill=fill)
        elif kind == "polygon":
            points = [tuple(float(part)*scale for part in point.split(",")) for point in attrs["points"].split()]
            draw.polygon(points, fill=fill)
        elif kind in ("ellipse", "circle"):
            x, y = float(attrs["cx"]), float(attrs["cy"])
            rx = float(attrs.get("rx", attrs.get("r")))
            ry = float(attrs.get("ry", attrs.get("r")))
            draw.ellipse(((x-rx)*scale, (y-ry)*scale, (x+rx)*scale, (y+ry)*scale), fill=fill,
                         outline=attrs.get("stroke"), width=max(1, int(float(attrs.get("stroke-width", 1))*scale)))
        elif kind == "polyline":
            points = [tuple(float(part)*scale for part in point.split(",")) for point in attrs["points"].split()]
            width = float(attrs["stroke-width"])*scale
            draw.line(points, fill=attrs["stroke"], width=int(width), joint="curve")
            for x, y in points:
                draw.ellipse((x-width/2, y-width/2, x+width/2, y+width/2), fill=attrs["stroke"])
    return image


def blend(a, b, weight):
    rgb = [round(int(a[i:i+2], 16)*(1-weight) + int(b[i:i+2], 16)*weight) for i in (1, 3, 5)]
    return "#" + "".join(f"{part:02x}" for part in rgb)


palettes = {
    "rule": ("#e6f6ef", "#54b99a", "#287e67", "#aedfcc", "#23866a"),
    "global": ("#f0ebfa", "#a18bd6", "#7453ae", "#d5c7f1", "#7955b4"),
    "direct": ("#eaf5fb", "#69a7d0", "#347cab", "#b9dbf1", "#327fad"),
    "idle": ("#eff1f4", "#a2aebb", "#768696", "#d5dfe9", "#768696"),
    "working": ("#fff2da", "#e5af56", "#ad782c", "#f9daa2", "#b77e2e"),
    "checking": ("#fff5df", "#e2b56a", "#aa8444", "#f4ddb4", "#af8640"),
    "attention": ("#fff0f2", "#db8c9a", "#a95468", "#f1c7d0", "#b8566d"),
}


def tray_variant(key):
    prefix, mode = key.split("-", 1) if "-" in key else ("", key)
    background, head, ear, inner, badge = palettes[mode]
    if prefix == "ready":
        head, ear, inner = blend(head, "#c6cbd3", .55), blend(ear, "#adb4c1", .55), blend(inner, "#e1e4e9", .55)
    if prefix == "external":
        background = "#f1eff8"
        head, ear, inner = blend(head, "#ada8c9", .8), "#908aaa", "#d4cee7"
    svg = deepcopy(master)
    colors = {"head": head, "trunk": head, "ear-left": ear, "ear-right": ear,
              "inner-left": inner, "inner-right": inner, "shine": blend(head, "#ffffff", .45),
              "cheek-left": blend(head, "#efd1d9", .7), "cheek-right": blend(head, "#efd1d9", .7)}
    for item in svg:
        if item.get("id") in colors:
            item.set("stroke" if item.get("id") == "trunk" else "fill", colors[item.get("id")])
    for item in list(svg):
        if item.get("id", "").startswith("eye-"):
            svg.remove(item)
    trunk = next(item for item in svg if item.get("id") == "trunk")
    if mode in ("idle", "attention"):
        trunk.set("points", "32,35 32,56")
    elif mode in ("global", "working"):
        trunk.set("points", "32,35 32,45 37,50 47,47 49,37")
        for item in svg:
            if item.get("id", "").startswith("ear-"):
                item.set("cy", "26")
    elif mode == "direct":
        trunk.set("points", "32,35 32,46 39,53 48,49 54,42")
    if prefix == "external" or mode == "checking":
        trunk.set("points", "32,35 32,48 26,55 18,51 16,45")
    if mode == "idle":
        svg.extend([line("21,27 24.5,30 28,27", "#405264", 2.6), line("36,27 39.5,30 43,27", "#405264", 2.6), line("49,5 55,5 49,11 55,11", "#768696")])
    else:
        x = 22.5 if prefix == "external" else 26 if mode == "direct" else 24.5
        svg.append(node("circle", cx=x, cy=27, r=2.35, fill="#203247"))
        if (mode in ("rule", "working", "checking") and not prefix):
            svg.append(line("36,27 39.5,30 43,27", "#203247", 2.6))
        else:
            svg.append(node("circle", cx=x+15, cy=27, r=2.35, fill="#203247"))
    if mode == "attention":
        svg.extend([line("21,22 28,20", "#724a57", 2), line("36,20 43,22", "#724a57", 2)])
    if mode != "idle":
        svg.append(node("circle", cx=51, cy=51, r=9, fill=badge))
        if prefix == "ready":
            svg.append(node("circle", cx=51, cy=51, r=5, fill="none", stroke="#ffffff", stroke_width=2))
        elif prefix == "external":
            svg.extend([line("46,48 56,48 53,45", "#ffffff", 1.8), line("56,54 46,54 49,57", "#ffffff", 1.8)])
        elif mode == "rule":
            svg.append(line("45,51 49,55 57,47", "#ffffff", 2.5))
        elif mode == "global":
            svg.extend([node("circle", cx=51, cy=51, r=6.5, fill="none", stroke="#ffffff", stroke_width=1.6), node("ellipse", cx=51, cy=51, rx=3, ry=6.5, fill="none", stroke="#ffffff", stroke_width=1.4), line("45,51 57,51", "#ffffff", 1.4)])
        elif mode == "direct":
            svg.extend([line("45,51 57,51", "#ffffff", 2), line("53,47 57,51 53,55", "#ffffff", 2)])
        elif mode == "working":
            svg.extend(node("circle", cx=x, cy=51, r=1.4, fill="#ffffff") for x in (46, 51, 56))
        elif mode == "attention":
            svg.extend([line("51,45 51,52", "#ffffff", 2.4), node("circle", cx=51, cy=56, r=1.4, fill="#ffffff")])
        else:
            svg.extend([line("47,47 50,45 54,46 55,49 51,52", "#ffffff", 2), node("circle", cx=51, cy=56, r=1.2, fill="#ffffff")])
    return svg


image = render(master).resize((256, 256), Image.Resampling.LANCZOS)
image.save(root / "mlash.png")
image.save(root / "mlash.ico", sizes=[(s, s) for s in (16, 24, 32, 48, 64, 128, 256)])
keys = ["idle", "working", "checking", "attention", "rule", "global", "direct", "ready-rule", "ready-global", "ready-direct", "external-rule", "external-global", "external-direct"]
tray = root / "tray"
tray.mkdir(exist_ok=True)
images = {}
for key in keys:
    svg = tray_variant(key)
    ET.ElementTree(svg).write(tray / f"{key}.svg", encoding="utf-8", xml_declaration=True)
    images[key] = render(svg)
    images[key].resize((64, 64), Image.Resampling.LANCZOS).save(tray / f"{key}.png")
qrc = ET.Element("RCC")
resource = ET.SubElement(qrc, "qresource", prefix="/brand")
ET.SubElement(resource, "file").text = "mlash.png"
for key in keys:
    ET.SubElement(resource, "file").text = f"tray/{key}.png"
ET.ElementTree(qrc).write(root / "brand.qrc", encoding="unicode")
preview = Image.new("RGB", (750, 580), "#f5f5f7")
draw = ImageDraw.Draw(preview)
font = ImageFont.load_default(size=12)
draw.text((16, 10), "Mlash - enlarged elephant / 16 & 24px on light & dark", fill="#45454d", font=font)
images["brand"] = render(master)
for index, key in enumerate(["brand"] + keys):
    x, y = (index % 5)*150, 40+(index // 5)*180
    preview.paste(images[key].resize((100, 100), Image.Resampling.LANCZOS), (x+25, y), images[key].resize((100, 100), Image.Resampling.LANCZOS))
    draw.text((x+12, y+108), key, fill="#45454d", font=font)
    draw.rounded_rectangle((x+10, y+130, x+138, y+165), 6, fill="#ffffff")
    draw.rectangle((x+75, y+130, x+138, y+165), fill="#292b31")
    for dx, size in ((16, 16), (42, 24), (82, 16), (108, 24)):
        small = images[key].resize((size, size), Image.Resampling.LANCZOS)
        preview.paste(small, (x+dx, y+147-size//2), small)
build = root.parent / "build"
build.mkdir(exist_ok=True)
preview.save(build / "tray-preview.png")
