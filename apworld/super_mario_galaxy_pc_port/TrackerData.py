"""What the tracker shows: the numbers behind the client's /tracker, /stars and /places commands.

Pure functions over what the server told the client (the slot data, the checked locations, the items received), so they
run without a connection.
"""
from typing import Any, Dict, Iterable, List, Mapping, NamedTuple, Optional

from . import DOME_NAMES, GRAND_STARS_IN_POOL
from .Items import GRAND_STAR, GREEN_STAR, POWER_STAR, item_table
from .Locations import all_stars
from .StarData import GALAXIES


class GalaxyRow(NamedTuple):
    stage: str
    name: str  # the galaxy
    place_name: str  # the place (miniature) it stands in; the galaxy's own name when nothing is shuffled
    room: str  # the Observatory room of the place ("" outside the domes)
    need: int  # the stars the place asks
    open_number: int  # the Grand Stars the place's room asks (0 outside the domes)
    checked: int
    total: int
    reachable_by_count: bool  # the star counter and the Grand Stars meet what the place asks


class Summary(NamedTuple):
    power: int
    green: int
    grand_items: int
    star_counter: int  # what the game counts: Power + Green + Grand Star items
    grand_flags: int  # the Grand Star flags: one more than the items (the game starts with the first)
    checked: int
    total: int
    rows: List[GalaxyRow]


def item_counts(received_ids: Iterable[int]) -> Dict[str, int]:
    by_code = {data.code: name for name, data in item_table.items()}
    counts: Dict[str, int] = {}
    for item_id in received_ids:
        name = by_code.get(item_id)
        if name is not None:
            counts[name] = counts.get(name, 0) + 1
    return counts


def summarize(slot_data: Optional[Mapping[str, Any]], checked_ids: Iterable[int], known_ids: Iterable[int],
              received_ids: Iterable[int]) -> Summary:
    """`known_ids`: every location the multiworld has for this slot (checked or not)."""
    slot_data = slot_data or {}
    checked = set(checked_ids)
    known = set(known_ids)
    counts = item_counts(received_ids)
    power, green, grand = counts.get(POWER_STAR, 0), counts.get(GREEN_STAR, 0), counts.get(GRAND_STAR, 0)
    counter = power + green + grand
    flags = grand + 1

    caps: List[int] = list(slot_data.get("galaxy_caps", []))
    order: List[int] = list(slot_data.get("dome_order", []))
    shuffle: List[int] = list(slot_data.get("galaxy_shuffle", []))
    place_of: Dict[str, str] = {}  # galaxy stage -> the place's stage
    for index, (stage, _name, _dome, _cap) in enumerate(GALAXIES):
        content_index = shuffle[index] if index < len(shuffle) else index
        content = GALAXIES[content_index][0] if 0 <= content_index < len(GALAXIES) else stage
        place_of[content] = stage

    index_of = {stage: i for i, (stage, _n, _d, _c) in enumerate(GALAXIES)}
    names = {stage: name for stage, name, _d, _c in GALAXIES}
    stars_by_stage: Dict[str, List[int]] = {}
    for star in all_stars:
        if star.code in known:
            stars_by_stage.setdefault(star.stage, []).append(star.code)

    rows: List[GalaxyRow] = []
    for stage, name, _dome, cap in GALAXIES:
        codes = stars_by_stage.get(stage)
        if not codes:
            continue
        place = place_of.get(stage, stage)
        place_index = index_of[place]
        place_dome = GALAXIES[place_index][2]
        need = caps[place_index] if place_index < len(caps) else GALAXIES[place_index][3]
        open_number = 0
        if place_dome:
            open_number = order[place_dome - 1] + 1 if place_dome - 1 < len(order) else place_dome
        rows.append(GalaxyRow(
            stage=stage, name=name, place_name=names[place], room=DOME_NAMES.get(place_dome, ""), need=need,
            open_number=open_number, checked=sum(1 for code in codes if code in checked), total=len(codes),
            reachable_by_count=counter >= need and flags >= open_number))
    return Summary(power, green, grand, counter, flags, len(checked & known), len(known), rows)


def format_summary(summary: Summary) -> List[str]:
    lines = [
        f"Stars: {summary.power} Power + {summary.green} Green + {summary.grand_items} Grand = {summary.star_counter} "
        f"(the star counter); Grand Star flags: {summary.grand_flags} of {GRAND_STARS_IN_POOL + 1}",
        f"Locations checked: {summary.checked} / {summary.total}",
    ]
    room = None
    for row in sorted(summary.rows, key=lambda r: (r.open_number == 0, r.open_number, r.need, r.name)):
        label = row.room or "Elsewhere"
        if label != room:
            room = label
            opens = f" - opens with Grand Star {row.open_number}" if row.open_number else ""
            lines.append(f"[{label}{opens}]")
        mark = "open" if row.reachable_by_count else f"needs {row.need} stars"
        where = f" (in {row.place_name}'s place)" if row.place_name != row.name else ""
        done = " DONE" if row.checked == row.total else ""
        lines.append(f"  {row.name}{where}: {row.checked}/{row.total}, {mark}{done}")
    return lines


def format_places(slot_data: Optional[Mapping[str, Any]]) -> List[str]:
    shuffled = (slot_data or {}).get("shuffled_galaxies") or {}
    if not shuffled:
        return ["No galaxy is shuffled in this slot."]
    return ["Miniature (what opens it) -> galaxy it shows"] + [f"  {place} -> {content}" for place, content in shuffled.items()]
