"""The locations of Super Mario Galaxy PC Port: its stars, one location each.

The stars come from StarData.py (the English names; the order is the numbering). The game checks a location when the
save file holds that star. A location's id is LOCATION_BASE + its number (the star's position in StarData.STARS, from 1);
kLocationBase in runtime/include/galaxy/archipelago.h.
"""
from typing import Dict, List, NamedTuple

from BaseClasses import Location

from .StarData import GALAXIES, STARS

LOCATION_BASE = 0x534D4700  # "SMG" and 0: kLocationBase

GATEWAY = "HeavensDoorGalaxy"
REACTOR = "KoopaBattleVs3Galaxy"
BOO_BONEYARD = "TeresaMario2DGalaxy"


class SMGLocation(Location):
    game = "Super Mario Galaxy PC Port"


LUIGI_OFFSET = 300  # Luigi's save has its own stars: LOCATION_BASE + LUIGI_OFFSET + number (kLuigiOffset in archipelago.h)
CHARACTERS = ("Mario", "Luigi")


class Star(NamedTuple):
    number: int  # the location number: LOCATION_BASE + number is Mario's id, LOCATION_BASE + LUIGI_OFFSET + number Luigi's
    stage: str
    scenario: int
    name: str  # the star's name, without the character
    kind: str
    character: str = "Mario"

    @property
    def code(self) -> int:
        return LOCATION_BASE + (LUIGI_OFFSET if self.character == "Luigi" else 0) + self.number

    @property
    def location_name(self) -> str:
        """The name of the location: the star and whose save it is in, like "Good Egg Galaxy - Dino Piranha (Mario)"."""
        return f"{self.name} ({self.character})"


# The Observatory's Star Bit shop: its shelves are locations too, SHOP_FIRST + n (kShopFirst, kShopMax in archipelago.h).
SHOP_FIRST = 201
SHOP_MAX = 40


def shop_location_name(n: int) -> str:
    """The name of the nth shelf (from 1)."""
    return f"Star Bit Shop - Item {n}"


all_stars: List[Star] = [Star(i + 1, *entry) for i, entry in enumerate(STARS)]  # Mario's
luigi_stars: List[Star] = [star._replace(character="Luigi") for star in all_stars]
every_star: List[Star] = all_stars + luigi_stars
location_table: Dict[str, int] = {star.location_name: star.code for star in every_star if star.kind != "ending"}
location_table.update({shop_location_name(n): LOCATION_BASE + SHOP_FIRST + n - 1 for n in range(1, SHOP_MAX + 1)})
galaxy_names: Dict[str, str] = {stage: name for stage, name, _dome, _cap in GALAXIES}
