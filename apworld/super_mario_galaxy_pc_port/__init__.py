import logging
from typing import Any, Dict, List

from BaseClasses import CollectionState, ItemClassification, Region, Tutorial
from Options import OptionError
from worlds.AutoWorld import WebWorld, World
from worlds.LauncherComponents import Component, Type, components, launch as launch_component
from worlds.generic.Rules import add_rule, set_rule

from .Items import (
    GREEN_STAR,
    GRAND_STAR,
    ONE_UP,
    POWER_STAR,
    RAINBOW_STAR,
    STAR_BITS,
    TRANSFORMATIONS,
    TRAPS,
    VICTORY,
    SMGItem,
    item_name_groups,
    item_table,
)
from .Locations import BOO_BONEYARD, GATEWAY, REACTOR, SHOP_FIRST, SMGLocation, Star, all_stars, galaxy_names, location_table, shop_location_name
from .Forms import FORM_STARS
from .Options import SMGOptions
from .StarData import GALAXIES

DOME_NAMES = {1: "Terrace", 2: "Fountain", 3: "Kitchen", 4: "Bedroom", 5: "Engine Room", 6: "Garden"}

GRAND_STARS_IN_POOL = 5  # the first one is given at the start (the game starts in the Observatory)
GREEN_STARS_IN_POOL = 3
COMET_STAR_COUNT = 13  # the stars the game asks before the Prankster Comets show up

# Galaxies outside the domes (and a few inside) open with more than the stars they ask: Grand Stars, the three Green
# Stars, or a particular star collected: only the planets of the Observatory (the four galaxies of the domes that asked a
# particular star - Buoy Base, Honeyclimb, Bonefin, Matter Splatter - open with the star count alone in this port).
# (Grand Stars needed, locations that must be reached.) The game's own rules,
# read from its event flag table (GameEventFlagTable.cpp).
EXTRA_REQUIREMENTS = {
    "BeltConveyerExGalaxy": (1, []),
    "CocoonExGalaxy": (2, ["Space Junk Galaxy - Tarantox's Tangled Web"]),
    "TearDropGalaxy": (3, ["Beach Bowl Galaxy - Sunken Treasure"]),
    "FishTunnelGalaxy": (4, ["Dusty Dune Galaxy - Soaring on the Desert Winds"]),
    "TransformationExGalaxy": (
        5,
        ["Ghostly Galaxy - Luigi and the Haunted Mansion", "Sea Slide Galaxy - Faster Than a Speeding Penguin"],
    ),
    "TeresaMario2DGalaxy": (6, ["Ghostly Galaxy - Luigi and the Haunted Mansion", "Gateway Galaxy - Gateway's Purple Coins"]),
    "SnowCapsuleGalaxy": (6, ["Melty Molten Galaxy - The Sinking Lava Spire"]),
}
# The galaxies of the domes by kind, for the galaxy shuffle: a Boss miniature is another kind of object in the game, so the
# Bosses only stand in each other's places.
MAJOR_GALAXIES = [
    "EggStarGalaxy", "HoneyBeeKingdomGalaxy", "StarDustGalaxy", "BattleShipGalaxy", "HeavenlyBeachGalaxy", "PhantomGalaxy",
    "CosmosGardenGalaxy", "IceVolcanoGalaxy", "SandClockGalaxy", "ReverseKingdomGalaxy", "OceanRingGalaxy", "FactoryGalaxy",
    "OceanPhantomCaveGalaxy", "CannonFleetGalaxy", "HellProminenceGalaxy",
]
MINOR_GALAXIES = [
    "SurfingLv1Galaxy", "FlipPanelExGalaxy", "TamakoroExLv1Galaxy", "BreakDownPlanetGalaxy", "CubeBubbleExLv1Galaxy",
    "OceanFloaterLandGalaxy", "HoneyBeeExGalaxy", "SkullSharkGalaxy", "DarkRoomGalaxy",
]
BOSS_GALAXIES = [
    "TriLegLv1Galaxy", "KoopaBattleVs1Galaxy", "KoopaJrShipLv1Galaxy", "KoopaBattleVs2Galaxy", "FloaterOtaKingGalaxy",
]
# The galaxies reached from the Observatory itself (the "?" planets that open with a flag or a story event, not a dome's
# miniature): another kind of object in the game, so they only stand in each other's places.
SPECIAL_GALAXIES = [
    "BeltConveyerExGalaxy", "CocoonExGalaxy", "TearDropGalaxy", "FishTunnelGalaxy", "TransformationExGalaxy",
    "TeresaMario2DGalaxy", "SnowCapsuleGalaxy", "TamakoroExLv2Galaxy", "SurfingLv2Galaxy", "CubeBubbleExLv2Galaxy",
]
MIN_SLACK = 6  # the room to leave (the game's own layout has about 3; see SMGWorld._exploration_slack)
TRIAL_GALAXIES = {"TamakoroExLv2Galaxy", "SurfingLv2Galaxy", "CubeBubbleExLv2Galaxy"}  # all three Green Stars


def launch_client(*args: str) -> None:
    from .Client import launch

    launch_component(launch, name="Super Mario Galaxy PC Port Client", args=args)


components.append(Component("Super Mario Galaxy PC Port Client", func=launch_client, component_type=Type.CLIENT,
                            game_name="Super Mario Galaxy PC Port"))


class SMGWebWorld(WebWorld):
    theme = "partyTime"
    tutorials = [
        Tutorial(
            "Multiworld Setup Guide",
            "A guide to playing Super Mario Galaxy on PC (the Nebula port) with Archipelago.",
            "English",
            "setup_en.md",
            "setup/en",
            ["Nebula Archipelago contributors"],
        )
    ]


def stars_total(state: CollectionState, player: int) -> int:
    """The game's star counter: the Power, Green and Grand Stars received (the Grand Star given at the start, which only
    opens the first dome, is not counted)."""
    return state.count(POWER_STAR, player) + state.count(GREEN_STAR, player) + state.count(GRAND_STAR, player)


def grand_total(state: CollectionState, player: int) -> int:
    """The Grand Star flags: one more than the items, for the first dome."""
    return state.count(GRAND_STAR, player) + 1


class SMGWorld(World):
    """
    Super Mario Galaxy is Mario's 2007 adventure across the cosmos: collect Power Stars in galaxies to power the Comet
    Observatory and reach Bowser. This world plays it through Nebula, the native PC port: every star is a location that
    sends an item to the multiworld, and the Power Stars, Grand Stars and Green Stars you receive are the ones the game
    counts. Grand Stars open the Observatory's domes.
    """

    game = "Super Mario Galaxy PC Port"
    web = SMGWebWorld()
    options_dataclass = SMGOptions
    options: SMGOptions

    item_name_to_id = {name: data.code for name, data in item_table.items()}
    location_name_to_id = dict(location_table)
    item_name_groups = item_name_groups
    ut_can_gen_without_yaml = True  # the Universal Tracker rebuilds the logic from the slot data, without the YAML

    stars: List[Star]
    open_position: Dict[int, int]
    content: Dict[str, str]
    power_stars: int
    stars_to_finish: int
    need: Dict[str, int]
    luma_counts: List[int]
    shop_prices: List[int]

    # ---- which stars are locations ----

    def _star_enabled(self, star: Star) -> bool:
        if star.kind == "ending":
            return False
        if star.stage == GATEWAY and star.scenario == 1:
            return False  # the Gateway Galaxy's Grand Star: given at the start
        if star.kind == "comet":
            return bool(self.options.include_comet_stars)
        if star.kind == "purple":
            return bool(self.options.include_purple_coin_stars)
        if star.stage == BOO_BONEYARD:
            return bool(self.options.include_purple_coin_stars)  # it opens with the Gateway's Purple Coin star
        if star.kind == "luigi" or (star.kind == "green" and star.stage == "BattleShipGalaxy"):
            return bool(self.options.include_luigi_stars)
        return True

    def _dome_offset_counts(self, limit: int) -> Dict[str, int]:
        """The stars each galaxy of the domes asks with the Dome 1-6 Access options: each orbit asks the previous
        orbit's number plus its offset (the first orbit of a dome, the largest number of the previous dome plus its
        offset), never more than `limit`."""
        names = ["dome_one_counts", "dome_two_counts", "dome_three_counts", "dome_four_counts", "dome_five_counts",
                 "dome_six_counts"]
        orbits = ["First Orbit", "Second Orbit", "Third Orbit", "Fourth Orbit", "Fifth Orbit"]
        counts: Dict[str, int] = {}
        previous_dome = 0
        for dome, option_name in enumerate(names, start=1):
            offsets = getattr(self.options, option_name).value
            previous_orbit = 0
            largest = 0
            for index, galaxy in enumerate([g for g in GALAXIES if g[2] == dome]):
                offset = 0 if (dome == 1 and index == 0) else int(offsets.get(orbits[index], 0))
                count = min(limit, (previous_dome if index == 0 else previous_orbit) + offset)
                counts[galaxy[0]] = count
                previous_orbit = count
                largest = max(largest, count)
            previous_dome = largest
        return counts

    def _luma_counts(self) -> List[int]:
        """The Star Bits each Hungry Luma asks (Dome 1 to 6, then the Gateway Galaxy's), or [] for the game's own."""
        behavior = self.options.observatory_hungry_luma_behavior
        if behavior.value == behavior.option_vanilla:
            return []
        keys = self.options.observatory_luma_counts.valid_keys
        values = self.options.observatory_luma_counts.value
        counts = [max(0, min(9999, int(values.get(key, 0)))) for key in keys]
        if behavior.value in (behavior.option_progressive_values_low, behavior.option_progressive_values_high):
            ceiling = 5000 if behavior.value == behavior.option_progressive_values_low else 9999
            last = -1
            grown = []
            for index, value in enumerate(sorted(counts)):
                value = min(value, ceiling)
                if index > 0 and value <= last:
                    value = self.random.randint(last + 1, ceiling) if last < ceiling else ceiling
                grown.append(value)
                last = value
            counts = grown
        return counts

    def _places_needs(self, base: Dict[str, int]) -> Dict[str, int]:
        """What the places (the miniatures) ask. A dome's places follow the order the domes open in: the nth place of
        the room that opens as dome d asks what the nth place of dome d asks."""
        need = dict(base)
        if not self.options.dome_shuffle:
            return need
        by_dome: Dict[int, List[int]] = {d: [base[g[0]] for g in GALAXIES if g[2] == d] for d in DOME_NAMES}
        for room, position in self.open_position.items():
            places = [g[0] for g in GALAXIES if g[2] == room]
            for index, stage in enumerate(places):
                caps = by_dome[position]
                need[stage] = caps[min(index, len(caps) - 1)]
        return need

    def _shuffle_domes(self) -> None:
        """self.open_position: room -> the Grand Star count (1 to 6) it opens with."""
        self.open_position = {room: room for room in DOME_NAMES}
        if self.options.dome_shuffle:
            # Any room can stand in the place of any other: the door that opens with the nth Grand Star leads to the room
            # that got that position (Engine Room in Terrace's place, Kitchen in Bedroom's, ...).
            positions = list(DOME_NAMES)
            self.random.shuffle(positions)
            self.open_position = dict(zip(DOME_NAMES, positions))

    def _shuffle_galaxies(self) -> None:
        """self.content: for each place (the vanilla galaxy's name), the galaxy that stands in it."""
        self.content = {stage: stage for stage, _name, _dome, _cap in GALAXIES}
        chosen = set(self.options.galaxy_shuffle.value)
        if "Full" in chosen:
            chosen |= {"Dome Majors", "Dome Minors", "Bosses", "Observatory Specials"}
        kind = self.options.galaxy_shuffle_type.value
        groups: List[List[str]] = []
        majors = list(MAJOR_GALAXIES) if "Dome Majors" in chosen else []
        minors = list(MINOR_GALAXIES) if "Dome Minors" in chosen else []
        bosses = list(BOSS_GALAXIES) if "Bosses" in chosen else []
        # Boo's Boneyard has no locations without the Purple Coin stars: it stays where it is, empty.
        specials = ([g for g in SPECIAL_GALAXIES if g != BOO_BONEYARD or self.options.include_purple_coin_stars]
                    if "Observatory Specials" in chosen else [])
        if kind == self.options.galaxy_shuffle_type.option_full:
            # Everything chosen in one pool: a dome can hold no boss or three, and the Observatory's galaxies (they have no
            # miniature of their own: a green sphere stands for them) can stand in a dome, a dome's galaxy in an Observatory planet.
            groups.append(majors + minors + bosses + specials)
        else:
            groups.extend([majors, minors, bosses, specials])
        groups = [places for places in groups if len(places) >= 2]
        if not groups:
            return
        best = None
        for _attempt in range(1000):
            for places in groups:
                galaxies = list(places)
                self.random.shuffle(galaxies)
                for place, galaxy in zip(places, galaxies):
                    self.content[place] = galaxy
            if self._has_circular_places():
                continue
            slack = self._exploration_slack()
            if best is None or slack > best[0]:
                best = (slack, dict(self.content))
            if slack >= MIN_SLACK:
                return
        if best is None:
            raise OptionError(f"Super Mario Galaxy PC Port ({self.player_name}): could not find a galaxy shuffle without a "
                              f"place that needs a star of the galaxy standing behind it.")
        self.content = best[1]  # the roomiest shuffle found

    def _exploration_slack(self) -> int:
        """How much room the shuffled map leaves for the fill: collecting a star in every location it can reach, the
        fewest locations left over, at any star count, once the stars found are taken out. Below 1 (a star count the
        reachable locations cannot hold) the map has a dead end where the fill fails."""
        counts: Dict[str, int] = {}
        for star in self.stars:
            if star.kind in ("star", "secret", "grand", "green") and not (star.kind == "green" and star.stage == "BattleShipGalaxy"):
                counts[star.stage] = counts.get(star.stage, 0) + 1
        places = [(stage, dome) for stage, _name, dome, _cap in GALAXIES if dome]
        highest = max(self.need[stage] for stage, _dome in places)
        worst = 1 << 30
        found = 0
        while found <= highest:
            grand = min(GRAND_STARS_IN_POOL, found) + 1
            reachable = sum(counts.get(self.content[stage], 0) for stage, dome in places
                            if self.open_position[dome] <= grand and self.need[stage] <= found)
            worst = min(worst, reachable - found)
            if reachable <= found:
                break
            found += 1
        return worst

    def _has_circular_places(self) -> bool:
        """A place that opens with a particular star (EXTRA_REQUIREMENTS) must not need a star of the galaxy that stands
        behind it, directly or through other places."""
        stage_of_star = {star.name: star.stage for star in all_stars}
        place_of = {content: place for place, content in self.content.items()}
        needs: Dict[str, set] = {}
        for place, (_grand, reach) in EXTRA_REQUIREMENTS.items():
            needs[place] = {place_of[stage_of_star[name]] for name in reach if name in stage_of_star}
        state: Dict[str, int] = {}

        def visit(place: str) -> bool:
            if state.get(place) == 1:
                return True
            if state.get(place) == 2:
                return False
            state[place] = 1
            if any(visit(other) for other in needs.get(place, ())):
                return True
            state[place] = 2
            return False

        return any(visit(place) for place in needs)

    def _regenerate_from_slot_data(self) -> bool:
        """The Universal Tracker builds the world again from the slot data of the multiworld that was generated: the
        shuffles, the dome order and what each place asks come from there, not from a new roll."""
        passthrough = getattr(self.multiworld, "re_gen_passthrough", None)
        if not isinstance(passthrough, dict) or self.game not in passthrough:
            return False
        slot_data = passthrough[self.game]
        options = slot_data.get("options", {})
        self.options.stars_to_finish.value = int(options.get("stars_to_finish", self.options.stars_to_finish.value))
        self.options.include_comet_stars.value = int(options.get("include_comet_stars", self.options.include_comet_stars.value))
        self.options.include_purple_coin_stars.value = int(
            options.get("include_purple_coin_stars", self.options.include_purple_coin_stars.value))
        self.options.include_luigi_stars.value = int(options.get("include_luigi_stars", self.options.include_luigi_stars.value))
        self.options.dome_shuffle.value = int(options.get("dome_shuffle", 0))
        self.options.transformation_items.value = int(options.get("transformation_items", 0))
        order = slot_data.get("dome_order", [room - 1 for room in DOME_NAMES])
        self.open_position = {room: int(order[room - 1]) + 1 for room in DOME_NAMES}
        caps = slot_data.get("galaxy_caps", [])
        self.need = {stage: int(caps[i]) if i < len(caps) else cap for i, (stage, _n, _d, cap) in enumerate(GALAXIES)}
        shuffle = slot_data.get("galaxy_shuffle", [])
        self.content = {stage: GALAXIES[int(shuffle[i])][0] if i < len(shuffle) else stage
                        for i, (stage, _n, _d, _c) in enumerate(GALAXIES)}
        self.shop_prices = [int(price) for price in slot_data.get("shop_prices", [])]
        return True

    def _shop_prices(self) -> List[int]:
        """The price of each shelf of the Star Bit shop: from the lowest to the highest, evenly, in steps of 5 Star Bits."""
        count = int(self.options.shop_items.value)
        low = int(self.options.shop_min_price.value)
        high = max(low, int(self.options.shop_max_price.value))
        if count <= 0:
            return []
        if count == 1:
            return [low]
        return [max(5, int(round((low + (high - low) * i / (count - 1)) / 5.0)) * 5) for i in range(count)]

    def interpret_slot_data(self, slot_data: Dict[str, Any]) -> Dict[str, Any]:
        """For the Universal Tracker: everything the tracker needs to rebuild the world is the slot data."""
        return slot_data

    def generate_early(self) -> None:
        regenerated = self._regenerate_from_slot_data()
        self.stars = [star for star in all_stars if self._star_enabled(star)]
        rest = len(self.stars) - GRAND_STARS_IN_POOL - GREEN_STARS_IN_POOL
        if self.options.transformation_items:
            rest -= len(TRANSFORMATIONS)  # their six items fill places too
        used_stages = {star.stage for star in self.stars}
        asked = self.options.stars_to_finish.value
        if not regenerated:
            self._shuffle_domes()
            # What each galaxy's place asks: the game's own numbers, or the Dome Access options' for the galaxies of the domes.
            base = {stage: cap for stage, _name, _dome, cap in GALAXIES}
            if self.options.star_requirements.value == self.options.star_requirements.option_dome_offsets:
                base.update(self._dome_offset_counts(min(asked, len(self.stars))))
            self.need = self._places_needs(base)
            self._shuffle_galaxies()
        highest = max(self.need[stage] for stage, content in self.content.items() if content in used_stages)
        # Enough Power Stars to reach the most demanding galaxy (the counter also holds the Green and Grand Stars).
        needed = max(0, highest - GRAND_STARS_IN_POOL - GREEN_STARS_IN_POOL)
        wanted = round(rest * self.options.power_star_percentage.value / 100)
        self.power_stars = min(rest, max(wanted, needed))
        if self.power_stars < needed:
            raise OptionError(f"Super Mario Galaxy PC Port ({self.player_name}): too few locations ({len(self.stars)}) to hold the "
                              f"Power Stars the galaxies ask for. Turn on more kinds of stars.")
        reachable = self.power_stars + GRAND_STARS_IN_POOL + GREEN_STARS_IN_POOL
        self.stars_to_finish = asked if regenerated else min(asked, reachable)
        self.luma_counts = self._luma_counts()
        if not regenerated:
            self.shop_prices = self._shop_prices()

    # ---- the map ----

    def _cap(self, stage: str, cap: int) -> int:
        return self.stars_to_finish if stage == REACTOR else self.need.get(stage, cap)

    def create_regions(self) -> None:
        player = self.player
        menu = Region("Menu", player, self.multiworld)
        observatory = Region("Comet Observatory", player, self.multiworld)
        menu.connect(observatory)
        regions = [menu, observatory]

        domes: Dict[int, Region] = {}
        for dome, name in DOME_NAMES.items():
            region = Region(f"Dome {dome}: {name}", player, self.multiworld)
            domes[dome] = region
            regions.append(region)
            observatory.connect(region, f"Dome {dome}",
                                lambda state, d=self.open_position[dome]: grand_total(state, player) >= d)

        by_stage: Dict[str, List[Star]] = {}
        for star in self.stars:
            by_stage.setdefault(star.stage, []).append(star)

        names = {stage: name for stage, name, _dome, _cap in GALAXIES}
        galaxy_regions: Dict[str, Region] = {}
        for stage, name, _dome, _cap in GALAXIES:
            if stage in by_stage or stage == REACTOR:
                region = Region(name, player, self.multiworld)
                galaxy_regions[stage] = region
                regions.append(region)

        for stage, place_name, dome, cap in GALAXIES:
            # `stage` is the place (the miniature that opens it); the galaxy that stands in it is the shuffle's choice.
            content = self.content[stage]
            if content not in galaxy_regions:
                continue
            region = galaxy_regions[content]
            name = place_name if content == stage else f"{place_name} place: {names[content]}"
            parent = domes[dome] if dome else observatory
            need = self._cap(stage, cap)
            grand_needed, reach = EXTRA_REQUIREMENTS.get(stage, (0, []))
            # Bowser's Galaxy Reactor opens with the star count alone (no Grand Star): the goal is its star.
            greens = GREEN_STARS_IN_POOL if stage in TRIAL_GALAXIES else 0

            def rule(state: CollectionState, need=need, grand=grand_needed, greens=greens, reach=tuple(reach)) -> bool:
                if stars_total(state, player) < need or grand_total(state, player) < grand:
                    return False
                if state.count(GREEN_STAR, player) < greens:
                    return False
                return all(self.get_location(location).can_reach(state) for location in reach)

            entrance = parent.connect(region, f"{name}", rule)
            self._extra_entrances.append((entrance, reach))

        for stage, region in galaxy_regions.items():
            for star in by_stage.get(stage, []):
                location = SMGLocation(player, star.name, star.code, region)
                region.locations.append(location)
                if star.kind == "comet":
                    add_rule(location, lambda state: stars_total(state, player) >= COMET_STAR_COUNT)
                elif star.kind == "purple":
                    add_rule(location, lambda state: state.has(VICTORY, player))
                # Luigi's stars need nothing more than their galaxy: the game puts Luigi in the scenario of his star.
                forms = FORM_STARS.get(star.name)
                if forms and self.options.transformation_items:
                    # A transformation that is an item: the star asks for it (any of the alternatives).
                    add_rule(location, lambda state, forms=forms: any(state.has(form, player) for form in forms))

        # The Star Bit shop: a shelf is a location of the Observatory (always reachable; the Star Bits are earned in play).
        for n in range(1, len(self.shop_prices) + 1):
            observatory.locations.append(SMGLocation(player, shop_location_name(n), location_table[shop_location_name(n)], observatory))

        # The goal is an event: the multiworld is complete when the player has "Victory". The game tells the client
        # when its goal is reached (the client sends the goal status).
        reactor = galaxy_regions[REACTOR]
        victory = SMGLocation(player, "Goal", None, reactor)
        victory.place_locked_item(SMGItem(VICTORY, ItemClassification.progression, None, player))
        reactor.locations.append(victory)

        self.multiworld.regions += regions

    _extra_entrances: List

    def __init__(self, *args, **kwargs) -> None:
        super().__init__(*args, **kwargs)
        self._extra_entrances = []

    def set_rules(self) -> None:
        register = getattr(self.multiworld, "register_indirect_condition", None)
        if register is not None:
            # A rule that asks whether a star can be reached depends on that star's region: tell the reachability cache.
            for entrance, reach in self._extra_entrances:
                for name in reach:
                    register(self.get_location(name).parent_region, entrance)
        self.multiworld.completion_condition[self.player] = lambda state: state.has(VICTORY, self.player)

    # ---- the items ----

    def create_item(self, name: str, classification: ItemClassification = None) -> SMGItem:
        data = item_table[name]
        return SMGItem(name, data.classification if classification is None else classification, data.code, self.player)

    def create_items(self) -> None:
        count = len(self.stars) + len(self.shop_prices)  # one item for each location (the goal event holds its own)
        pool: List[str] = [GRAND_STAR] * GRAND_STARS_IN_POOL + [GREEN_STAR] * GREEN_STARS_IN_POOL + [POWER_STAR] * self.power_stars
        if self.options.transformation_items:
            pool += list(TRANSFORMATIONS)
        filler = count - len(pool)
        traps = round(filler * self.options.trap_percentage.value / 100)
        rainbows = round((filler - traps) * self.options.rainbow_star_percentage.value / 100)
        pool += [RAINBOW_STAR] * rainbows
        filler -= rainbows
        one_ups = round((filler - traps) * self.options.one_up_percentage.value / 100)
        kinds = [name for name in TRAPS if name in self.options.trap_types.value] or list(TRAPS)
        pool += [kinds[self.random.randrange(len(kinds))] for _ in range(traps)]
        pool += [ONE_UP] * one_ups + [STAR_BITS] * (filler - traps - one_ups)
        # Only the Power Stars the logic can ever ask for count as progression (the star counter also holds the Green and
        # Grand Stars); the others are still stars the game counts, but the fill need not place them with care.
        required = max(0, max(self.stars_to_finish, max(self.need.values())) - GRAND_STARS_IN_POOL - GREEN_STARS_IN_POOL)
        items = []
        power_made = 0
        for name in pool:
            if name == POWER_STAR:
                power_made += 1
                items.append(self.create_item(name, ItemClassification.progression if power_made <= required else ItemClassification.useful))
            else:
                items.append(self.create_item(name))
        self.multiworld.itempool += items

    def generate_basic(self) -> None:
        # With shuffled places the first sphere can be as small as one galaxy's few stars: a Power Star early keeps the
        # fill from running into a dead end there.
        early = self.multiworld.early_items[self.player]
        if self.options.dome_shuffle or self.content_shuffled():
            early[POWER_STAR] = max(early.get(POWER_STAR, 0), 1)
        if self.options.transformation_items:
            # Forms.py lists only the stars whose mission is the transformation: have all of them early as well.
            for name in TRANSFORMATIONS:
                early[name] = max(early.get(name, 0), 1)

    def content_shuffled(self) -> bool:
        return any(content != stage for stage, content in self.content.items())

    def get_filler_item_name(self) -> str:
        return STAR_BITS

    # ---- what the game is told ----

    def fill_slot_data(self) -> Dict[str, Any]:
        # Read by the game's host (galaxy/archipelago.h: Option, the lists): the options by name, the most stars each
        # galaxy asks (the game uses the smaller of that and its own requirement, so this logic never asks less than
        # the game does), the dome order.
        slot_data: Dict[str, Any] = {
            "goal": 0,
            "galaxy_caps": [self._cap(stage, cap) for stage, _name, _dome, cap in GALAXIES],
            "dome_order": [self.open_position[room] - 1 for room in DOME_NAMES],
            "options": {
                "goal": 0,
                "star_items": 1,
                "start_at_observatory": 1,
                "dome_shuffle": int(bool(self.options.dome_shuffle)),
                "stars_to_finish": self.stars_to_finish,
                "include_comet_stars": int(bool(self.options.include_comet_stars)),
                "include_purple_coin_stars": int(bool(self.options.include_purple_coin_stars)),
                "include_luigi_stars": int(bool(self.options.include_luigi_stars)),
                "exact_requirements": int(self.options.star_requirements.value == self.options.star_requirements.option_dome_offsets
                                          or bool(self.options.dome_shuffle)),
                "luma_counts_set": 1 if self.luma_counts else 0,
                "star_colors": int(bool(self.options.star_colors)),
                "transformation_items": int(bool(self.options.transformation_items)),
                "death_link": int(bool(self.options.death_link)),
                "trap_percentage": int(self.options.trap_percentage.value),
                "music_shuffle": int(bool(self.options.music_shuffle)),
                "enemy_shuffle": int(bool(self.options.enemy_shuffle)),
            },
        }
        if self.content_shuffled():
            index = {stage: number for number, (stage, _n, _d, _c) in enumerate(GALAXIES)}
            slot_data["galaxy_shuffle"] = [index[self.content[stage]] for stage, _n, _d, _c in GALAXIES]
            slot_data["shuffled_galaxies"] = {
                place_name: next(name for stage2, name, _d, _c in GALAXIES if stage2 == self.content[stage])
                for stage, place_name, _d, _c in GALAXIES if self.content[stage] != stage
            }
        if self.luma_counts:
            slot_data["luma_counts"] = self.luma_counts
        if self.shop_prices:
            slot_data["shop_prices"] = self.shop_prices
        return slot_data
