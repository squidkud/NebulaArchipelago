# Super Mario Galaxy PC Port

## Where is the options page?

The [player options page for this game](../player-options) holds the options you need to make a config file.

## What does randomization do to this game?

The levels do not change. What changes is what collecting a star gives you. Every star of the game is a location with
its own name ("Good Egg Galaxy - Dino Piranha"), and it sends an item to the multiworld. The stars you play with are
the ones the multiworld sends you:

- **Power Stars**, **Green Stars** and **Grand Stars** are items. The game's star counter, the galaxies' star
  requirements and the Grand Star / Green Star events all read the items you have received, not the stars you
  collected.
- **Grand Stars open the Observatory's domes.** You start with the first one, which opens the Terrace dome but does not
  count in the star counter; the five in the item pool open the Fountain, Kitchen, Bedroom, Engine Room and Garden
  domes in that order, and count as stars.
- You start in the Comet Observatory as soon as you pick a save file, with no need for a finished game.
- **`character`** (Mario, Luigi or Both) says who you play. Each star exists once per character and is named after it
  ("Good Egg Galaxy - Dino Piranha (Mario)", "... (Luigi)"). With Mario or Luigi the game forces that character on every
  save file and the Mario/Luigi switch at the file select is hidden; with Both, the switch stays and both characters'
  stars are locations. Mario's and Luigi's saves share the same received items and star counter.

## What is considered a location check?

Each star of the 41 galaxies, by name and character: the normal stars, the Green Stars, and the Prankster Comet stars (optional).
Purple Coin stars (optional, goal Bowser only) can be added. The kinds that are off (comet, Purple Coin) are not shown in the game's star select. The Grand Stars
you collect in the game are locations too, except the first one and the ending.

## Which items can be in another world's pool?

Your own items are Power Stars, Green Stars, Grand Stars, 1-Up Mushrooms, 50 Star Bits and, with `trap_percentage`
above 0, seven traps: Fire (Mario burns), Shock (an electric shock), Freeze (Mario is frozen) and Spike (a hurting hit), each
costing a health point, so one can take a life; Star Bit Thief (up to 50 Star Bits are taken); Quake (the camera shakes
for ten seconds) and Confusion (the control stick is inverted for twenty seconds). `trap_types` picks which can come.
A trap that cannot start yet waits until it can.

The rest of the filler is fixed (not an option): mostly 1-Up Mushrooms and Star Bits, with a few packs of 200 Star Bits,
bursts of three 1-Up Mushrooms, Full Heals (health filled at once) and Rainbow Stars (Mario turns rainbow for a while),
all rare.

The moves (Spin, Wall Jump...) are not items in this version: the option was taken out until the logic knows which star needs which move.

## What happens when I receive an item?

Stars count straight away. While you play (not in a cutscene, a menu or after a death) the game hands you one 1-Up
Mushroom or 50 Star Bits at a time. A short message on screen tells you what the multiworld said.

## What is the goal?

Take the star of the final boss in Bowser's Galaxy Reactor. The goal is sent the moment you take it (no need to watch
the epilogue and the credits). The number of stars set by "Stars to Finish" only opens that last galaxy (Rosetta's talk
about harmony); no Grand Star is needed.

With `goal: bowser_and_120_stars` the game also needs every star that exists before the ending (Comet, Secret, Green and
Grand stars). The Purple Coin stars only open after the ending, so they are not asked. With this goal Stars to Finish, Include Comet Stars and Include Purple Coin Stars are ignored (everything is on).

With `goal: 242_stars` the game needs every star location of the characters you play, Purple Coin stars included (the ending
goal is not enough on its own). With `character: both` this is the 242 star run. The goal is sent when every chosen
character has finished it. The last star of each character is Grand Finale Galaxy - The Star Festival (the 121st star, a location only with this goal): the game's own entrance is
unchanged (it opens only once both Mario and Luigi have their 120 stars), the logic asks every other star of the character, and
the galaxy can also be entered from the ADVANCED galaxy select of the settings. The goal is sent by whoever takes theirs last. These two goals also ignore the three star options above.

## Extra options

`enemy_shuffle` (experimental, untested in play) swaps plain enemies for other kinds, never bosses or enemies a mission counts or that follow a path or switch. `music_shuffle` plays another theme for each stage and boss theme (jingles, cutscene songs and sound effects
stay). With `galaxy_shuffle_type: full` every chosen kind of galaxy is in one pool, so a dome can hold no boss or three
and the Observatory's galaxies can stand in the domes (the place keeps its own miniature model, with the name, stars and launch of the galaxy that stands in it). A dome's galaxies are always shown (no "???") and the four that
asked for a particular star (Buoy Base, Honeyclimb, Bonefin, Matter Splatter) open with their star count alone.
`skybox_shuffle` (experimental, untested in play) swaps the skies of the stages for other skies. `star_colors` paints each star after the class of the item it hides (light blue `#5CBEFF` normal, blue `#6D8BE8` useful, purple `#897EC9` progression, red
trap; Grand Stars unchanged). `death_link` turns on DeathLink: a life lost is sent to the other DeathLink players, and
their deaths kill Mario here.

`shop_items` (1 to 20, default 8), `shop_min_price` (default 100) and `shop_max_price` (default 2000) set the Observatory's Star Bit shop: the yellow Luma
at the start of the Observatory (the one that opens Nebula's achievements page) opens it instead. Each shelf is a
location that holds an item of the multiworld (its name and player are shown once connected) and costs Star Bits; A
buys the shelf under the cursor, B closes. You must be connected to buy, and a purchase is kept by the server. Prices are
not part of the logic (the game assumes you can collect the Star Bits). The shop is always there.

`transformation_items` (off by default) turns the six transformations (Bee Mushroom, Boo Mushroom, Spring Mushroom, Fire Flower,
Ice Flower, Flying/Red Star) into items: taking the power-up in a galaxy does nothing until you have received its item
(Rainbow Mario is not affected). The logic does not know which star needs which form, so the six items are placed early.

`star_requirements: dome_offsets` sets what each galaxy of the domes asks from the Dome 1-6 Access options, and the
Observatory Hungry Luma options set the Star Bits each Hungry Luma asks.

## The client and the tracker

The Archipelago Launcher lists a "Super Mario Galaxy PC Port Client". The game itself talks to the server through Nebula
(archipelago.ini); this client only watches the same slot, so you can leave it open next to the game. Its commands:
`/tracker` (the star counter, the Grand Stars, every galaxy with its stars checked and whether its star count is met),
`/stars <part of a galaxy's name>` (its stars, checked or not), `/places` (which galaxy each miniature shows when the
galaxies are shuffled). With the Universal Tracker installed the client uses its window too: the world rebuilds its logic
from the slot data (shuffles, dome order, star counts), so the tracker shows what is in logic.

## Shuffles

`galaxy_shuffle` (Dome Majors, Dome Minors, Bosses or Full) swaps the galaxies the Observatory's miniatures show: a
miniature keeps what opens it (the stars it asks, its place in its dome) but shows, names and launches another galaxy,
and that galaxy's stars are the locations. Bosses only trade places with Bosses. `galaxy_shuffle_type` picks By Type,
Major Separate or Full (Majors and Minors mixed). `dome_shuffle` puts a room in the place of another (Engine Room where
Terrace is, Kitchen where Bedroom is, ...): the door that opens with the nth Grand Star leads to another room, and the
stars a room asks follow the order the rooms open in. Both are experimental.

## Not done yet

The Mario/Luigi colours, Hide Galaxy, Star Colors, and the by-sphere Luma mode of the
original Dolphin world are not in this version.

Use a new save file for each multiworld.

## Mario's and Luigi's colours

`mario_cap_color`, `mario_shirt_color`, `mario_overalls_color`, `mario_gloves_color`, `mario_shoes_color` and the same with
`luigi_` recolour the character for you only. Each takes a name (`red`, `orange`, `yellow`, `green`, `azure`, `blue`, `purple`,
`pink`, `brown`, `white`, `black`, `gold`...), a hex code (`ff8000`, `#ff8000`) or `255,128,0`; empty keeps the game's own colour.
The game recolours the character's pictures in memory: the cap and the shirt are what is red on Mario (green on Luigi), the overalls
what is blue; the gloves and shoes are recoloured whole, found by the name of their picture. The cap and the shirt share a picture,
so the cap colour is used for both unless the game keeps them apart. Not tested in play.
