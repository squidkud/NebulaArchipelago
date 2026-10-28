"""The items of Super Mario Galaxy PC Port, and their numbers.

The numbers are shared with the game's side: kItemBase and the kItem* in runtime/include/galaxy/archipelago.h (and
NEBULA_AP_ITEM_* in native/include/nebula_game_api.h). An item's id is ITEM_BASE + its number; change one side and the
other must follow.
"""
from typing import Dict, NamedTuple

from BaseClasses import Item, ItemClassification

ITEM_BASE = 0x534D4800  # "SMH" and 0: kItemBase

ONE_UP = "1-Up Mushroom"
STAR_BITS = "50 Star Bits"
POWER_STAR = "Power Star"
GREEN_STAR = "Green Star"
GRAND_STAR = "Grand Star"
FIRE_TRAP = "Fire Trap"
SHOCK_TRAP = "Shock Trap"
FREEZE_TRAP = "Freeze Trap"
SPIKE_TRAP = "Spike Trap"
STAR_BIT_THIEF = "Star Bit Thief"
QUAKE_TRAP = "Quake Trap"
CONFUSION_TRAP = "Confusion Trap"
TRAPS = (FIRE_TRAP, SHOCK_TRAP, FREEZE_TRAP, SPIKE_TRAP, STAR_BIT_THIEF, QUAKE_TRAP, CONFUSION_TRAP)
RAINBOW_STAR = "Rainbow Star"
BEE_MUSHROOM = "Bee Mushroom"
BOO_MUSHROOM = "Boo Mushroom"
SPRING_MUSHROOM = "Spring Mushroom"
FIRE_FLOWER = "Fire Flower"
ICE_FLOWER = "Ice Flower"
FLYING_STAR = "Flying (Red Star)"
TRANSFORMATIONS = (BEE_MUSHROOM, BOO_MUSHROOM, SPRING_MUSHROOM, FIRE_FLOWER, ICE_FLOWER, FLYING_STAR)
VICTORY = "Victory"  # the event the goal places; never sent to the game


class SMGItem(Item):
    game = "Super Mario Galaxy PC Port"


class ItemData(NamedTuple):
    code: int
    classification: ItemClassification


item_table: Dict[str, ItemData] = {
    ONE_UP: ItemData(ITEM_BASE + 1, ItemClassification.filler),
    STAR_BITS: ItemData(ITEM_BASE + 2, ItemClassification.filler),
    POWER_STAR: ItemData(ITEM_BASE + 3, ItemClassification.progression),
    GREEN_STAR: ItemData(ITEM_BASE + 4, ItemClassification.progression),
    GRAND_STAR: ItemData(ITEM_BASE + 5, ItemClassification.progression),
    RAINBOW_STAR: ItemData(ITEM_BASE + 13, ItemClassification.useful),
    BEE_MUSHROOM: ItemData(ITEM_BASE + 14, ItemClassification.progression),
    BOO_MUSHROOM: ItemData(ITEM_BASE + 15, ItemClassification.progression),
    SPRING_MUSHROOM: ItemData(ITEM_BASE + 16, ItemClassification.progression),
    FIRE_FLOWER: ItemData(ITEM_BASE + 17, ItemClassification.progression),
    ICE_FLOWER: ItemData(ITEM_BASE + 18, ItemClassification.progression),
    FLYING_STAR: ItemData(ITEM_BASE + 19, ItemClassification.progression),
    FIRE_TRAP: ItemData(ITEM_BASE + 6, ItemClassification.trap),
    SHOCK_TRAP: ItemData(ITEM_BASE + 7, ItemClassification.trap),
    FREEZE_TRAP: ItemData(ITEM_BASE + 8, ItemClassification.trap),
    SPIKE_TRAP: ItemData(ITEM_BASE + 9, ItemClassification.trap),
    STAR_BIT_THIEF: ItemData(ITEM_BASE + 10, ItemClassification.trap),
    QUAKE_TRAP: ItemData(ITEM_BASE + 11, ItemClassification.trap),
    CONFUSION_TRAP: ItemData(ITEM_BASE + 12, ItemClassification.trap),
}

item_name_groups = {
    "Stars": {POWER_STAR, GREEN_STAR, GRAND_STAR},
    "Traps": set(TRAPS),
    "Transformations": set(TRANSFORMATIONS),
}
