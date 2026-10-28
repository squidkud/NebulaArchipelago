from dataclasses import dataclass
from typing import Any

from Options import (
    Choice,
    DeathLink,
    OptionCounter,
    OptionError,
    OptionSet,
    PerGameCommonOptions,
    Range,
    Toggle,
    random_weighted_range,
)


TRAP_NAMES = ("Fire Trap", "Shock Trap", "Freeze Trap", "Spike Trap", "Star Bit Thief", "Quake Trap", "Confusion Trap")  # Items.TRAPS


class RandomOptionCounter(OptionCounter):
    """An OptionCounter whose values also accept the random texts: random, random-low, random-high,
    random-range-5-20, low, high."""

    def __init__(self, value: dict[str, Any]) -> None:
        values: dict[str, int] = {}
        for key, amount in value.items():
            if isinstance(amount, int):
                values[key] = amount
            elif isinstance(amount, str):
                text = amount.lower().strip()
                if text == "high":
                    values[key] = self.max
                elif text == "low":
                    values[key] = self.min
                elif text.startswith("random"):
                    mode = "random"
                    low, high = self.min, self.max
                    if text.startswith("random-range-"):
                        parts = text.split("-")
                        try:
                            low, high = int(parts[-2]), int(parts[-1])
                        except ValueError:
                            raise OptionError("Invalid range text for " + self.__class__.__name__ + ": " + amount)
                        if low < self.min or high > self.max or low > high:
                            raise OptionError(
                                "The range " + amount + " must lie between " + str(self.min) + " and " + str(self.max)
                                + " for " + self.__class__.__name__
                            )
                        if len(parts) > 2 and parts[2] in ("low", "middle", "high"):
                            mode = "random-" + parts[2]
                    values[key] = random_weighted_range(mode, low, high)
                else:
                    raise OptionError("Invalid value " + repr(amount) + " for " + self.__class__.__name__)
            else:
                raise OptionError("Invalid value " + repr(amount) + " for " + self.__class__.__name__)
        super().__init__(values)


class StarsToFinish(Range):
    """How many stars (Power, Green and Grand Stars received, counted as the game's star counter counts them) it takes
    to open Bowser's Galaxy Reactor (Rosetta's talk about harmony, then you can go). The game's own requirement stays
    if it is lower. No Grand Star is needed. The goal is the star of the final boss: it is sent as soon as you take it."""
    display_name = "Stars to Finish"
    range_start = 10
    range_end = 100
    default = 50


class PowerStarPercentage(Range):
    """Of the locations that are not taken by Grand Stars and Green Stars, the share that holds a Power Star. The
    others hold 1-Up Mushrooms and Star Bits. At least enough Power Stars to open every galaxy are always added."""
    display_name = "Power Star Percentage"
    range_start = 50
    range_end = 100
    default = 80


class IncludeCometStars(Toggle):
    """Whether the Prankster Comet stars (Daredevil, Speedy, Cosmic Mario and Fast Foes missions) are locations."""
    display_name = "Include Comet Stars"
    default = 1


class IncludePurpleCoinStars(Toggle):
    """Whether the Purple Coin stars are locations. The game offers them after the ending, so these are only
    reachable once you have beaten Bowser. This also adds Boo's Boneyard, which needs the Gateway Galaxy's Purple Coin
    star to open."""
    display_name = "Include Purple Coin Stars"
    default = 0


class IncludeLuigiStars(Toggle):
    """Whether the stars of Luigi's hide-and-seek (Good Egg, Honeyhive and the Green Star of Battlerock) are
    locations. You pick them in the star select like any other star: Luigi is in the galaxy as soon as you choose that
    star (no letter, no need to have rescued him first)."""
    display_name = "Include Luigi Hide-and-Seek Stars"
    default = 1


class OneUpPercentage(Range):
    """Of the items that fill your own world's locations after the stars, the share that are 1-Up Mushrooms; the others
    are 50 Star Bits. Items from other worlds are not affected."""
    display_name = "1-Up Mushroom Percentage"
    range_start = 0
    range_end = 100
    default = 50


class ShopItems(Range):
    """How many items the Observatory's Star Bit shop sells. The Luma at the start of the Observatory (where the game
    keeps its achievements) opens it: each shelf is a location that holds an item of the multiworld and costs Star Bits.
    The shop is always there (at least one shelf)."""
    display_name = "Star Bit Shop Items"
    range_start = 1
    range_end = 20
    default = 8


class ShopMinPrice(Range):
    """The Star Bits the cheapest shelf of the shop costs."""
    display_name = "Star Bit Shop Lowest Price"
    range_start = 5
    range_end = 5000
    default = 100


class ShopMaxPrice(Range):
    """The Star Bits the dearest shelf of the shop costs (the shelves in between grow evenly from the lowest price)."""
    display_name = "Star Bit Shop Highest Price"
    range_start = 5
    range_end = 9999
    default = 2000


class StarRequirements(Choice):
    """How many stars each galaxy of the domes asks to open.

    vanilla: the game's own numbers.
    dome_offsets: the numbers come from the Dome 1 to 6 Access options below (the game's numbers are not used)."""
    display_name = "Galaxy Star Requirements"
    option_vanilla = 0
    option_dome_offsets = 1
    default = 0


def _dome_offsets(number: int, name: str, vanilla: str, maximum: int, first_orbit: bool):
    keys = ["First Orbit", "Second Orbit", "Third Orbit", "Fourth Orbit", "Fifth Orbit"]
    if number == 6:
        keys = keys[:4]
    if not first_orbit:
        keys = keys[1:]
    first_note = "" if first_orbit else "The first orbit in Dome 1 always has no star requirement.\n    "
    doc = (
        "Star requirements of each galaxy of Dome %d, which is the %s in vanilla. Used when \"Galaxy Star "
        "Requirements\" is dome_offsets.\n    Each number is how many more stars are needed than for the previous orbit "
        "(the first orbit of a dome adds to the largest number of the previous dome), capped by Stars to Finish. "
        "%sThe numbers can also be random, random-range-1-5 and so on. The range is 0 to %d."
    ) % (number, vanilla, first_note, maximum)
    return type(
        "Dome%dOffsets" % number,
        (RandomOptionCounter,),
        {
            "__doc__": doc,
            "display_name": "Dome %d Access" % number,
            "min": 0,
            "max": maximum,
            "valid_keys": keys,
            "default": {key: 0 for key in keys},
        },
    )


Dome1Offsets = _dome_offsets(1, "Dome 1", "Terrace", 8, False)
Dome2Offsets = _dome_offsets(2, "Dome 2", "Fountain", 10, True)
Dome3Offsets = _dome_offsets(3, "Dome 3", "Kitchen", 10, True)
Dome4Offsets = _dome_offsets(4, "Dome 4", "Bedroom", 10, True)
Dome5Offsets = _dome_offsets(5, "Dome 5", "Engine Room", 10, True)
Dome6Offsets = _dome_offsets(6, "Dome 6", "Garden", 20, True)


class ObservatoryHungryLumaBehavior(Choice):
    """How many Star Bits the Observatory's Hungry Lumas ask.

    vanilla: the game's numbers.
    choice: exactly the numbers of Observatory Luma Counts.
    progressive_values_low: like choice, but the numbers only grow from Luma to Luma (a number that is not higher than
    the previous Luma's is rolled higher), up to 5000.
    progressive_values_high: the same, up to 9999."""
    display_name = "Observatory Hungry Luma Behavior"
    option_vanilla = 0
    option_choice = 1
    option_progressive_values_low = 3
    option_progressive_values_high = 4
    default = 0


class ObservatoryLumaCounts(RandomOptionCounter):
    """The Star Bits each Hungry Luma of the Observatory asks (used unless Observatory Hungry Luma Behavior is vanilla).
    Maximum 9999. The numbers can also be random, random-range-100-900 and so on."""
    display_name = "Observatory Luma Counts"
    min = 0
    max = 9999
    valid_keys = [
        "Dome 1 Hungry Luma",
        "Dome 2 Hungry Luma",
        "Dome 3 Hungry Luma",
        "Dome 4 Hungry Luma",
        "Dome 5 Hungry Luma",
        "Dome 6 Hungry Luma",
        "Gateway Hungry Luma",
    ]
    default = {
        "Dome 1 Hungry Luma": 400,
        "Dome 2 Hungry Luma": 400,
        "Dome 3 Hungry Luma": 600,
        "Dome 4 Hungry Luma": 800,
        "Dome 5 Hungry Luma": 1000,
        "Dome 6 Hungry Luma": 1600,
        "Gateway Hungry Luma": 1200,
    }


class GalaxyShuffle(OptionSet):
    """Which galaxies of the domes are shuffled between the miniature galaxies of the Observatory. Leave empty for none.

    Dome Majors: the 15 big galaxies (Good Egg, Honeyhive, Space Junk, ...).
    Dome Minors: the 9 small ones (Loopdeeloop, Flipswitch, Rolling Green, ...).
    Bosses: the 5 Bowser and Bowser Jr. galaxies that give a Grand Star (they stand in each other's places, unless the
    type is Full).
    Observatory Specials: the 10 galaxies reached from the Observatory itself (Sweet Sweet, Sling Pod, Drip Drop, Bigmouth,
    Sand Spiral, Boo's Boneyard, Snow Cap and the three Green Star galaxies). They stand in each other's places, unless
    the type is Full.
    Full: all four.

    A miniature keeps what opens it (the stars it asks, its place in its dome); the galaxy it shows, names and launches
    is the shuffled one."""
    display_name = "Galaxy Shuffle"
    valid_keys = {"Full", "Dome Majors", "Dome Minors", "Observatory Specials", "Bosses"}
    default = frozenset()


class GalaxyShuffleType(Choice):
    """How the galaxies are shuffled. Does nothing when Galaxy Shuffle is empty.

    By Type: Majors only among Major places, Minors among Minor places, Bosses among Boss places, Specials among
    Special places.
    Major Separate: the same (kept for old YAML files).
    Full: every kind chosen in Galaxy Shuffle in ONE pool, any galaxy in any place: a dome can hold no boss or three,
    and the galaxies of the Hungry Lumas and the other Observatory planets can stand in the domes (the place keeps its
    own miniature model), a galaxy of a dome in an Observatory planet's place too."""
    display_name = "Galaxy Shuffle Type"
    option_by_type = 0
    option_major_separate = 1
    option_full = 2
    default = 0


class DomeShuffle(Toggle):
    """Shuffle the Observatory's rooms: a room stands in the place of another (Engine Room where Terrace is, Kitchen
    where Bedroom is, ...). The door that opens with the nth Grand Star leads to another room; the galaxies of a room
    stay in it, and the stars they ask follow the order the rooms open in."""
    display_name = "Dome Shuffle"


class TrapPercentage(Range):
    """Of the items that fill your own world's locations after the stars, the share that are traps (a trap that cannot
    start yet - Mario is hurt, in a cutscene - waits until it can). The traps: Fire (Mario burns), Shock (an
    electric shock), Freeze (Mario is frozen) and Spike (a hit that hurts), each costing a health point, so one can
    take a life; Star Bit Thief (up to 50 Star Bits are taken); Quake (the camera shakes for ten seconds); Confusion
    (the control stick is inverted for twenty seconds). Trap Types picks which ones can come. The rest is split
    between 1-Up Mushrooms and Star Bits as One-Up Percentage says."""
    display_name = "Trap Percentage"
    range_start = 0
    range_end = 100
    default = 0


class RainbowStarPercentage(Range):
    """Of the items that fill your own world's locations after the stars and the traps, the share that are Rainbow Stars:
    Mario turns rainbow (invincible) for a while. It is the only bonus item; the rest is split between 1-Up Mushrooms
    and Star Bits as One-Up Percentage says."""
    display_name = "Rainbow Star Percentage"
    range_start = 0
    range_end = 100
    default = 0


class MusicShuffle(Toggle):
    """Shuffle the music: each theme of the stages and bosses plays another one (the same multiworld shuffles the same
    way; the jingles, the cutscene songs and every sound effect stay as they are)."""
    display_name = "Music Shuffle"


class EnemyShuffle(Toggle):
    """Swap the plain enemies of the stages for other kinds (Goombas, Bob-ombs, Octoombas, Fire Boos, Piranha Plants,
    Monty Moles, Swoopins; the same multiworld swaps them the same way). Only enemies placed with nothing attached to them
    are swapped - never the bosses, the other creatures, or an enemy that a mission counts, follows a path or waits for a
    switch. Experimental: not tested in play."""
    display_name = "Enemy Shuffle"


class TrapTypes(OptionSet):
    """The traps that can fill the Trap Percentage: any of Fire Trap, Shock Trap, Freeze Trap, Spike Trap, Star Bit
    Thief, Quake Trap, Confusion Trap. All by default."""
    display_name = "Trap Types"
    valid_keys = frozenset(TRAP_NAMES)
    default = frozenset(TRAP_NAMES)


class StarColors(Toggle):
    """The colour of a star in the level (and in the star select) tells the class of the item it hides: green
    is a normal item, blue is useful, gold is progression and red is a trap. Grand Stars keep their look. The game asks the
    server what each location holds when it connects, so the colours are there for the whole session."""
    display_name = "Star Colors by Item Class"


class TransformationItems(Toggle):
    """The transformations become items. Bee Mushroom, Boo Mushroom, Spring Mushroom, Fire Flower, Ice Flower and Flying
    (the Red Star) are in the multiworld: taking one of those power-ups in a galaxy does nothing until you have received
    its item. Rainbow Mario is not affected. The logic knows the stars whose whole mission is a transformation (Honeyhive, Honeyclimb,
    Boo's Boneyard, Dusty Dune's first star, Cake Lane, Freezeflame); the six items are also placed early, because it
    does not know the other stars where a power-up only helps (come back later if a star needs one you lack)."""
    display_name = "Transformations as Items"


@dataclass
class SMGOptions(PerGameCommonOptions):
    stars_to_finish: StarsToFinish
    power_star_percentage: PowerStarPercentage
    include_comet_stars: IncludeCometStars
    include_purple_coin_stars: IncludePurpleCoinStars
    include_luigi_stars: IncludeLuigiStars
    one_up_percentage: OneUpPercentage
    shop_items: ShopItems
    shop_min_price: ShopMinPrice
    shop_max_price: ShopMaxPrice
    star_requirements: StarRequirements
    dome_one_counts: Dome1Offsets
    dome_two_counts: Dome2Offsets
    dome_three_counts: Dome3Offsets
    dome_four_counts: Dome4Offsets
    dome_five_counts: Dome5Offsets
    dome_six_counts: Dome6Offsets
    observatory_hungry_luma_behavior: ObservatoryHungryLumaBehavior
    observatory_luma_counts: ObservatoryLumaCounts
    galaxy_shuffle: GalaxyShuffle
    galaxy_shuffle_type: GalaxyShuffleType
    dome_shuffle: DomeShuffle
    star_colors: StarColors
    transformation_items: TransformationItems
    trap_percentage: TrapPercentage
    trap_types: TrapTypes
    rainbow_star_percentage: RainbowStarPercentage
    music_shuffle: MusicShuffle
    enemy_shuffle: EnemyShuffle
    death_link: DeathLink
