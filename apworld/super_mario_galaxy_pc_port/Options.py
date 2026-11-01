from dataclasses import dataclass
from typing import Any

from Options import (
    Choice,
    DeathLink,
    FreeText,
    OptionCounter,
    OptionError,
    OptionGroup,
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


class Character(Choice):
    """Who plays. Mario or Luigi: the game keeps that character on every save file (the file select has no button to
    switch) and the locations are the stars of that character's saves. Both: Mario's and Luigi's saves each have their
    own stars, all of them locations ("... (Mario)" and "... (Luigi)"); the file select keeps its button, and the goal
    has to be reached with each of them. The stars received (Power, Green and Grand Stars) count for both saves."""
    display_name = "Character"
    option_mario = 0
    option_luigi = 1
    option_both = 2
    default = 0


class StarsToFinish(Range):
    """Only used with the goal Bowser (ignored with the goals Bowser And 120 Stars and 242 Stars). How many stars (Power, Green and Grand Stars
    received, counted as the game's star counter counts them) it takes to open Bowser's Galaxy Reactor (Rosetta's talk
    about harmony, then you can go). The game's own requirement stays if it is lower. No Grand Star is needed. The goal
    is the star of the final boss: it is sent as soon as you take it."""
    display_name = "Stars to Finish"
    range_start = 10
    range_end = 100
    default = 50


class IncludeCometStars(Toggle):
    """Only used with the goal Bowser (always on with the goals Bowser And 120 Stars and 242 Stars). Whether the Prankster Comet stars (Daredevil,
    Speedy, Cosmic Mario and Fast Foes missions) are locations. When off, they are not shown in the star select either."""
    display_name = "Include Comet Stars"
    default = 1


class IncludePurpleCoinStars(Toggle):
    """Only used with the goal Bowser (always on with the goals Bowser And 120 Stars and 242 Stars). Whether the Purple Coin stars are locations.
    The game offers them after the ending, so these are only reachable once you have beaten Bowser. When off, they are
    not shown in the star select either."""
    display_name = "Include Purple Coin Stars"
    default = 0


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
    (the control stick is inverted for twenty seconds). Trap Types picks which ones can come. The rest of the filler
    (1-Ups, Star Bits, Full Heals, Rainbow Stars...) is the same in every world."""
    display_name = "Trap Percentage"
    range_start = 0
    range_end = 100
    default = 0



class Goal(Choice):
    """What finishes the game. Bowser: the star of Bowser's Galaxy Reactor (the normal ending). Bowser And 120 Stars: that
    star, and every star that exists before the ending too (the Comet, Secret, Green, Grand and Luigi stars; the Purple
    Coin stars only open after the ending, so they do not count). 242 Stars: the ending and every star there is, the
    Purple Coin stars too (with both characters: the stars of Mario's saves and of Luigi's), plus the Grand Finale Galaxy's Star
    Festival, the 121st star of each character, the last one (the game's own entrance opens only when Mario and Luigi both have 120 stars; the settings' ADVANCED galaxy select goes there at any time). The multiworld stays
    unfinished until it is done, with each character the slot plays. With Bowser And 120 Stars or 242 Stars, Stars to
    Finish, Include Comet Stars and Include Purple Coin Stars are ignored: all the stars are in."""
    display_name = "Goal"
    option_bowser = 0
    option_bowser_and_120_stars = 2
    option_242_stars = 3
    alias_all_stars = 3
    default = 0


class MusicShuffle(Toggle):
    """Shuffle the music: each theme of the stages and bosses plays another one (the same multiworld shuffles the same
    way; the jingles, the cutscene songs and every sound effect stay as they are)."""
    display_name = "Music Shuffle"


class SkyboxShuffle(Toggle):
    """Swap the skies of the stages for other skies (the same multiworld swaps them the same way): the plain skies among
    themselves (including the summer, retro and dark-space skies, Bowser Jr.'s fight and the children's room), the skies
    drawn with a projection map among themselves (including the Observatory's, Bowser's fight and the title screen's),
    and the three skies inside the domes among themselves. Experimental: not tested in play."""
    display_name = "Skybox Shuffle"


class EnemyShuffle(Toggle):
    """Swap the plain enemies of the stages for other kinds, each family among itself so that the new enemy suits the
    place: the ground enemies (Goombas, Bob-ombs, Octoombas, Octoguys, Piranha Plants, Monty Moles, Dry Bones, hopping
    spiders, which may also become a Fire Boo) and the airborne ones (Swoopins and Fire Boos, with their ice kinds). The
    same multiworld swaps them the same way. Only enemies placed with nothing attached to them are swapped - never the
    bosses, the other creatures, or an enemy that a mission counts, follows a path or waits for a switch. Experimental:
    not tested in play."""
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


COLOR_NAMES = {
    "red": 0xE02020, "orange": 0xFF8000, "yellow": 0xFFD800, "lime": 0x80E000, "green": 0x20B020, "teal": 0x00A090,
    "cyan": 0x00D0F0, "azure": 0x0080FF, "blue": 0x2040E0, "navy": 0x182080, "purple": 0x8020D0, "violet": 0x8020D0,
    "magenta": 0xF020F0, "pink": 0xFF70B0, "brown": 0x704020, "tan": 0xC8A070, "white": 0xFFFFFF, "gray": 0x808080,
    "grey": 0x808080, "black": 0x181818, "gold": 0xF0C000, "silver": 0xC0C0C0,
}
# The slot_data options color_0 .. color_9 (galaxy/archipelago.h: Option::ColorFirst + n): Mario's five parts, then Luigi's.
COLOR_OPTION_NAMES = (
    "mario_cap_color", "mario_shirt_color", "mario_overalls_color", "mario_gloves_color", "mario_shoes_color",
    "luigi_cap_color", "luigi_shirt_color", "luigi_overalls_color", "luigi_gloves_color", "luigi_shoes_color",
)


def parse_color(text: str) -> int | None:
    """A colour as 0xRRGGBB: a name from COLOR_NAMES, "RRGGBB" or "#RRGGBB", or "R,G,B" (0 to 255 each). None when the text
    is empty (the part keeps the game's colour); raises ValueError for a text that is none of those."""
    text = str(text).strip().lower()
    if not text or text in ("default", "none", "off", "vanilla"):
        return None
    if text in COLOR_NAMES:
        return COLOR_NAMES[text]
    if "," in text:
        parts = [part.strip() for part in text.split(",")]
        if len(parts) == 3 and all(part.isdigit() and int(part) <= 255 for part in parts):
            return (int(parts[0]) << 16) | (int(parts[1]) << 8) | int(parts[2])
        raise ValueError(text)
    digits = text.lstrip("#")
    if len(digits) == 6 and all(c in "0123456789abcdef" for c in digits):
        return int(digits, 16)
    raise ValueError(text)


class PlayerColor(FreeText):
    """A colour for this part of the character: a name (red, orange, yellow, green, azure, blue, purple, pink, brown, white,
    black, gold...), a hex code like ff8000 or #ff8000, or three numbers like 255,128,0. Empty keeps the game's colour. Only
    you see it. Mario and Luigi pictures are recoloured in memory: the cap and the shirt are the parts that are red on Mario
    (green on Luigi) and the overalls those that are blue; the gloves and the shoes are recoloured whole."""
    default = ""


class MarioCapColor(PlayerColor):
    """Mario's cap (and shirt, when no shirt colour is set). The cap and the shirt share their texture in the game, so
    when both are set the cap's colour is used for both unless the game keeps them apart."""
    display_name = "Mario Cap Color"


class MarioShirtColor(PlayerColor):
    """Mario's shirt (only when the game keeps it apart from the cap)."""
    display_name = "Mario Shirt Color"


class MarioOverallsColor(PlayerColor):
    """Mario's overalls."""
    display_name = "Mario Overalls Color"


class MarioGlovesColor(PlayerColor):
    """Mario's gloves."""
    display_name = "Mario Gloves Color"


class MarioShoesColor(PlayerColor):
    """Mario's shoes."""
    display_name = "Mario Shoes Color"


class LuigiCapColor(PlayerColor):
    """Luigi's cap (and shirt, when no shirt colour is set)."""
    display_name = "Luigi Cap Color"


class LuigiShirtColor(PlayerColor):
    """Luigi's shirt (only when the game keeps it apart from the cap)."""
    display_name = "Luigi Shirt Color"


class LuigiOverallsColor(PlayerColor):
    """Luigi's overalls."""
    display_name = "Luigi Overalls Color"


class LuigiGlovesColor(PlayerColor):
    """Luigi's gloves."""
    display_name = "Luigi Gloves Color"


class LuigiShoesColor(PlayerColor):
    """Luigi's shoes."""
    display_name = "Luigi Shoes Color"


@dataclass
class SMGOptions(PerGameCommonOptions):
    # The order of the options here is the order of the template YAML; GROUPS (below) names the sections.
    character: Character
    goal: Goal
    stars_to_finish: StarsToFinish
    include_comet_stars: IncludeCometStars
    include_purple_coin_stars: IncludePurpleCoinStars
    star_requirements: StarRequirements
    dome_one_counts: Dome1Offsets
    dome_two_counts: Dome2Offsets
    dome_three_counts: Dome3Offsets
    dome_four_counts: Dome4Offsets
    dome_five_counts: Dome5Offsets
    dome_six_counts: Dome6Offsets
    observatory_hungry_luma_behavior: ObservatoryHungryLumaBehavior
    observatory_luma_counts: ObservatoryLumaCounts
    transformation_items: TransformationItems
    trap_percentage: TrapPercentage
    trap_types: TrapTypes
    shop_items: ShopItems
    shop_min_price: ShopMinPrice
    shop_max_price: ShopMaxPrice
    dome_shuffle: DomeShuffle
    galaxy_shuffle: GalaxyShuffle
    galaxy_shuffle_type: GalaxyShuffleType
    music_shuffle: MusicShuffle
    enemy_shuffle: EnemyShuffle
    skybox_shuffle: SkyboxShuffle
    star_colors: StarColors
    death_link: DeathLink
    mario_cap_color: MarioCapColor
    mario_shirt_color: MarioShirtColor
    mario_overalls_color: MarioOverallsColor
    mario_gloves_color: MarioGlovesColor
    mario_shoes_color: MarioShoesColor
    luigi_cap_color: LuigiCapColor
    luigi_shirt_color: LuigiShirtColor
    luigi_overalls_color: LuigiOverallsColor
    luigi_gloves_color: LuigiGlovesColor
    luigi_shoes_color: LuigiShoesColor


GROUPS = [
    OptionGroup("Goal and Stars", [Character, Goal, StarsToFinish, IncludeCometStars, IncludePurpleCoinStars]),
    OptionGroup("Galaxy Access", [StarRequirements, Dome1Offsets, Dome2Offsets, Dome3Offsets, Dome4Offsets, Dome5Offsets,
                                  Dome6Offsets, ObservatoryHungryLumaBehavior, ObservatoryLumaCounts]),
    OptionGroup("Items", [TransformationItems]),
    OptionGroup("Traps", [TrapPercentage, TrapTypes]),
    OptionGroup("Star Bit Shop", [ShopItems, ShopMinPrice, ShopMaxPrice]),
    OptionGroup("Shuffles", [DomeShuffle, GalaxyShuffle, GalaxyShuffleType, MusicShuffle, EnemyShuffle, SkyboxShuffle]),
    OptionGroup("Display and Multiplayer", [StarColors, DeathLink]),
    OptionGroup("Mario Colors", [MarioCapColor, MarioShirtColor, MarioOverallsColor, MarioGlovesColor, MarioShoesColor]),
    OptionGroup("Luigi Colors", [LuigiCapColor, LuigiShirtColor, LuigiOverallsColor, LuigiGlovesColor, LuigiShoesColor]),
]
