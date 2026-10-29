#!/usr/bin/env python3
"""Builds the PopTracker pack of Super Mario Galaxy PC Port (Archipelago auto-tracking).

    python tools/gen_poptracker_pack.py [output.zip]      (default: SMG-PC-Port-PopTracker.zip next to this file's repo root)

Everything comes from the APWorld (apworld/super_mario_galaxy_pc_port): the stars and galaxies from StarData.py, the item
numbers from Items.py. Put the zip in PopTracker's packs folder, open it, and connect to the Archipelago server from
PopTracker's "Archipelago" button. The map is a plain board with one square per galaxy (the game has no map to show);
hover a square to see its stars; a square turns green when all its stars are checked.
"""
import importlib.util
import json
import sys
import types
import zipfile
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parent.parent
WORLD = ROOT / "apworld" / "super_mario_galaxy_pc_port"
ICONS = Path(__file__).resolve().parent / "item_icons"
VERSION = "1.0.0"


def load_world_modules():
    """StarData.py and Items.py without the Archipelago install: Items only needs BaseClasses' two names."""
    base = types.ModuleType("BaseClasses")

    class _Item:
        pass

    class _Classification:
        filler = progression = useful = trap = 0

    base.Item = _Item
    base.ItemClassification = _Classification
    sys.modules.setdefault("BaseClasses", base)
    modules = {}
    for name in ("StarData", "Items"):
        spec = importlib.util.spec_from_file_location(f"smg_{name}", WORLD / f"{name}.py")
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        modules[name] = module
    return modules["StarData"], modules["Items"]


star_data, items = load_world_modules()

# (code, display name, picture or None for a generated badge, kind)
#   kind: "toggle" (have it or not) or "count" (how many)
ITEM_ROWS = [
    ("power_star", items.POWER_STAR, "power_star", "count"),
    ("green_star", items.GREEN_STAR, "green_star", "count"),
    ("grand_star", items.GRAND_STAR, "grand_star", "count"),
    ("bee", items.BEE_MUSHROOM, "bee_mushroom", "toggle"),
    ("boo", items.BOO_MUSHROOM, "boo_mushroom", "toggle"),
    ("spring", items.SPRING_MUSHROOM, "spring_mushroom", "toggle"),
    ("fire", items.FIRE_FLOWER, "fire_flower", "toggle"),
    ("ice", items.ICE_FLOWER, "ice_flower", "toggle"),
    ("flying", items.FLYING_STAR, "flying_star", "toggle"),
    ("rainbow", items.RAINBOW_STAR, "rainbow_star", "toggle"),
]
ITEM_BASE = items.ITEM_BASE
ITEM_CODES = {name: data.code for name, data in items.item_table.items()}

CELL_W, CELL_H, COLUMNS = 168, 74, 7


def font(size):
    for candidate in ("DejaVuSans-Bold.ttf", "arialbd.ttf", "Arial Bold.ttf"):
        try:
            return ImageFont.truetype(candidate, size)
        except OSError:
            pass
    return ImageFont.load_default()


def icon_for(code, name, picture):
    image = Image.new("RGBA", (48, 48), (0, 0, 0, 0))
    if picture and (ICONS / f"{picture}.png").exists():
        source = Image.open(ICONS / f"{picture}.png").convert("RGBA")
        box = source.getchannel("A").point(lambda a: 255 if a > 8 else 0).getbbox()
        if box:
            source = source.crop(box)
        source.thumbnail((44, 44))
        image.paste(source, ((48 - source.width) // 2, (48 - source.height) // 2), source)
        return image
    draw = ImageDraw.Draw(image)
    draw.rounded_rectangle((2, 2, 45, 45), 9, fill=(64, 92, 168, 255), outline=(255, 255, 255, 255), width=2)
    words = name.replace("(", "").replace(")", "").split()
    lines = [words[0], " ".join(words[1:])] if len(words) > 1 else [words[0]]
    y = 24 - 7 * len(lines)
    for line in lines:
        f = font(11)
        w = draw.textlength(line, font=f)
        draw.text((24 - w / 2, y), line, font=f, fill=(255, 255, 255, 255))
        y += 14
    return image


def grey(image):
    out = image.convert("LA").convert("RGBA")
    out.putalpha(image.getchannel("A").point(lambda a: a * 45 // 100))
    return out


def png_bytes(image):
    import io
    buf = io.BytesIO()
    image.save(buf, "PNG")
    return buf.getvalue()


def build(output):
    files = {}

    # ---- items
    item_json = []
    for code, name, picture, kind in ITEM_ROWS:
        icon = icon_for(code, name, picture)
        files[f"images/items/{code}.png"] = png_bytes(icon)
        entry = {"name": name, "codes": code, "img": f"images/items/{code}.png"}
        if kind == "count":
            entry.update({"type": "consumable", "min_quantity": 0, "max_quantity": 120, "increment": 1, "decrement": 1,
                          "initial_quantity": 0, "display_name": name})
            entry["disabled_img"] = f"images/items/{code}_off.png"
            files[f"images/items/{code}_off.png"] = png_bytes(grey(icon))
        else:
            entry.update({"type": "toggle", "disabled_img_mods": "@disabled", "img_mods": ""})
            entry["disabled_img"] = f"images/items/{code}_off.png"
            files[f"images/items/{code}_off.png"] = png_bytes(grey(icon))
            entry.pop("disabled_img_mods")
            entry.pop("img_mods")
        item_json.append(entry)
    files["items/items.json"] = json.dumps(item_json, indent=2)

    # ---- the board: one square per galaxy
    galaxies = star_data.GALAXIES
    rows = (len(galaxies) + COLUMNS - 1) // COLUMNS
    board = Image.new("RGBA", (COLUMNS * CELL_W, rows * CELL_H), (14, 18, 40, 255))
    draw = ImageDraw.Draw(board)
    small = font(12)
    stars_by_stage = {}
    for number, (stage, scenario, name, kind) in enumerate(star_data.STARS, start=1):
        if kind == "ending":
            continue
        stars_by_stage.setdefault(stage, []).append((number, name))
    locations = []
    location_ids = {}
    for index, (stage, galaxy_name, dome, cap) in enumerate(galaxies):
        stars = stars_by_stage.get(stage)
        col, row = index % COLUMNS, index // COLUMNS
        x0, y0 = col * CELL_W, row * CELL_H
        draw.rectangle((x0 + 3, y0 + 3, x0 + CELL_W - 4, y0 + CELL_H - 4), outline=(90, 110, 190, 255), width=2)
        label = galaxy_name
        if len(label) > 22:
            cut = label.rfind(" ", 0, 22)
            first, second = label[:cut], label[cut + 1:]
        else:
            first, second = label, ""
        draw.text((x0 + 10, y0 + 8), first, font=small, fill=(230, 235, 255, 255))
        if second:
            draw.text((x0 + 10, y0 + 24), second, font=small, fill=(230, 235, 255, 255))
        domelabel = f"Dome {dome}" if dome else "Special"
        draw.text((x0 + 10, y0 + CELL_H - 22), domelabel, font=font(10), fill=(140, 155, 210, 255))
        if not stars:
            continue
        sections = []
        for character, offset in (("Mario", 0), ("Luigi", 300)):  # Luigi's save has its own stars (kLuigiOffset)
            for number, star_name in stars:
                section = f"{star_name} ({character})"
                sections.append({"name": section, "item_count": 1})
                location_ids[0x534D4700 + offset + number] = f"@{galaxy_name}/{section}"
        locations.append({
            "name": galaxy_name,
            "sections": sections,
            "map_locations": [{"map": "galaxies", "x": x0 + CELL_W - 22, "y": y0 + CELL_H - 18}],
        })
    files["images/maps/galaxies.png"] = png_bytes(board)
    files["maps/maps.json"] = json.dumps([{"name": "galaxies", "location_size": 22, "location_border_thickness": 2,
                                           "img": "images/maps/galaxies.png"}], indent=2)
    files["locations/locations.json"] = json.dumps(locations, indent=2)

    # ---- layouts
    grid = {"type": "itemgrid", "item_margin": "4,4", "rows": [[code for code, *_ in ITEM_ROWS[:3]],
                                                               [code for code, *_ in ITEM_ROWS[3:10]],
                                                               [code for code, *_ in ITEM_ROWS[10:]]]}
    layout = {
        "tracker_default": {"type": "container", "background": "#0e1228",
                            "content": {"type": "dock", "content": [
                                {"type": "dock", "dock": "top", "content": grid},
                                {"type": "map", "maps": ["galaxies"]}]}},
        "tracker_horizontal": {"type": "container", "background": "#0e1228",
                               "content": {"type": "dock", "content": [
                                   {"type": "dock", "dock": "left", "content": grid},
                                   {"type": "map", "maps": ["galaxies"]}]}},
    }
    files["layouts/tracker.json"] = json.dumps(layout, indent=2)
    files["layouts/broadcast.json"] = json.dumps({"tracker_broadcast": {"type": "container", "content": grid}}, indent=2)
    files["layouts/layouts.json"] = json.dumps({}, indent=2)  # placeholder so the folder exists in every PopTracker version

    # ---- scripts
    item_map = {}
    for code, name, _picture, kind in ITEM_ROWS:
        item_map[ITEM_CODES[name]] = (code, kind)
    lua = ["-- Generated by tools/gen_poptracker_pack.py: do not edit by hand.", "ITEM_MAPPING = {"]
    for number, (code, kind) in sorted(item_map.items()):
        lua.append(f'    [{number}] = {{"{code}", "{kind}"}},')
    lua.append("}")
    lua.append("LOCATION_MAPPING = {")
    for number, path in sorted(location_ids.items()):
        lua.append(f'    [{number}] = "{path}",')
    lua.append("}")
    files["scripts/mapping.lua"] = "\n".join(lua) + "\n"
    files["scripts/init.lua"] = '''ScriptHost:LoadScript("scripts/mapping.lua")
Tracker:AddItems("items/items.json")
Tracker:AddMaps("maps/maps.json")
Tracker:AddLocations("locations/locations.json")
Tracker:AddLayouts("layouts/tracker.json")
Tracker:AddLayouts("layouts/broadcast.json")
ScriptHost:LoadScript("scripts/autotracking.lua")
'''
    files["scripts/autotracking.lua"] = '''-- Archipelago auto-tracking: items received and locations checked, straight from the server.
CUR_INDEX = -1

local function reset()
    CUR_INDEX = -1
    for _, mapped in pairs(ITEM_MAPPING) do
        local obj = Tracker:FindObjectForCode(mapped[1])
        if obj then
            if mapped[2] == "count" then
                obj.AcquiredCount = 0
            else
                obj.Active = false
            end
        end
    end
    for _, path in pairs(LOCATION_MAPPING) do
        local obj = Tracker:FindObjectForCode(path)
        if obj then
            obj.AvailableChestCount = obj.ChestCount
        end
    end
end

function onClear(slot_data)
    reset()
end

function onItem(index, item_id, item_name, player_number)
    if index <= CUR_INDEX then
        return
    end
    CUR_INDEX = index
    local mapped = ITEM_MAPPING[item_id]
    if not mapped then
        return
    end
    local obj = Tracker:FindObjectForCode(mapped[1])
    if not obj then
        return
    end
    if mapped[2] == "count" then
        obj.AcquiredCount = obj.AcquiredCount + 1
    else
        obj.Active = true
    end
end

function onLocation(location_id, location_name)
    local path = LOCATION_MAPPING[location_id]
    if not path then
        return
    end
    local obj = Tracker:FindObjectForCode(path)
    if obj then
        obj.AvailableChestCount = obj.AvailableChestCount - 1
    end
end

Archipelago:AddClearHandler("smg clear", onClear)
Archipelago:AddItemHandler("smg item", onItem)
Archipelago:AddLocationHandler("smg location", onLocation)
'''
    files["manifest.json"] = json.dumps({
        "name": "Super Mario Galaxy PC Port (Nebula Archipelago)",
        "game_name": "Super Mario Galaxy PC Port",
        "package_uid": "nebula_archipelago_smg",
        "package_version": VERSION,
        "platform": "pc",
        "target_object_version": "1.0.0",
        "min_poptracker_version": "0.25.0",
        "variants": {"standard": {"display_name": "Galaxy board", "flags": ["ap"]}},
    }, indent=2)
    files["README.txt"] = ("PopTracker pack for Super Mario Galaxy PC Port (Nebula Archipelago).\n"
                           "Put this zip into PopTracker's packs folder, load it, then use the Archipelago button to connect.\n"
                           "Generated by tools/gen_poptracker_pack.py from the APWorld, version " + VERSION + ".\n")

    output = Path(output)
    with zipfile.ZipFile(output, "w", zipfile.ZIP_DEFLATED) as archive:
        for path, data in sorted(files.items()):
            archive.writestr(path, data)
    return len(files), len(location_ids)


if __name__ == "__main__":
    target = sys.argv[1] if len(sys.argv) > 1 else str(ROOT / "SMG-PC-Port-PopTracker.zip")
    count, stars = build(target)
    print(f"wrote {target}: {count} files, {stars} star locations")
