"""The stars that need a transformation (used when the "Transformations as Items" option is on).

FORM_STARS: star name -> what the logic asks for it. It is a tuple of groups; the player needs at least one item of EVERY
group, so ((FIRE_FLOWER,), (ICE_FLOWER,)) asks for both and ((BEE_MUSHROOM, BOO_MUSHROOM),) asks for either one.

OUT_OF_LOGIC: star name -> the transformation(s) the star can be done without. The logic still asks for the item; this is only
information for the player (the docs and the client's /stars command), it is never required or counted as reachable.
"""
from .Items import BEE_MUSHROOM, BOO_MUSHROOM, FIRE_FLOWER, FLYING_STAR, ICE_FLOWER, SPRING_MUSHROOM

BEE = ((BEE_MUSHROOM,),)
BOO = ((BOO_MUSHROOM,),)
FIRE = ((FIRE_FLOWER,),)
ICE = ((ICE_FLOWER,),)
SPRING = ((SPRING_MUSHROOM,),)
FLYING = ((FLYING_STAR,),)

FORM_STARS = {
    # Bee Mushroom
    "Honeyhive Galaxy - Bee Mario Takes Flight": BEE,
    "Honeyhive Galaxy - Big Bad Bugaboom": BEE,
    "Honeyclimb Galaxy - Scaling the Sticky Wall": BEE,
    "Gold Leaf Galaxy - Star Bunnies on the Hunt": BEE,
    "Gold Leaf Galaxy - Cataquack to the Skies": BEE,
    "Gold Leaf Galaxy - When It Rains, It Pours": BEE,
    "Sea Slide Galaxy - The Silver Stars of Sea Slide": BEE,
    "Sea Slide Galaxy - Purple Coins by the Seaside": BEE,
    # Bee Mushroom or Boo Mushroom (either one is enough)
    "Sand Spiral Galaxy - Choosing a Favorite Snack": ((BEE_MUSHROOM, BOO_MUSHROOM),),
    # Boo Mushroom
    "Ghostly Galaxy - Luigi and the Haunted Mansion": BOO,
    "Boo's Boneyard Galaxy - Racing the Spooky Speedster": BOO,
    # Fire Flower
    "Freezeflame Galaxy - Freezeflame's Blistering Core": FIRE,
    "Freezeflame Galaxy - Conquering the Summit": FIRE,
    "Deep Dark Galaxy - The Underground Ghost Ship": FIRE,
    "Snow Cap Galaxy - Star Bunnies in the Snow": FIRE,
    # Fire Flower and Ice Flower
    "Freezeflame Galaxy - Hot and Cold Collide": FIRE + ICE,
    "Freezeflame Galaxy - Purple Coins on the Summit": FIRE + ICE,
    "Deep Dark Galaxy - Bubble Blastoff": FIRE + ICE,
    # Ice Flower
    "Beach Bowl Galaxy - Wall Jumping Up Waterfalls": ICE,
    "Freezeflame Galaxy - The Frozen Peak of Baron Brr": ICE,
    # Spring Mushroom
    "Toy Time Galaxy - Heavy Metal Mecha-Bowser": SPRING,
    "Toy Time Galaxy - Bouncing Down Cake Lane": SPRING,
    "Matter Splatter Galaxy - Watch Your Step": SPRING,
    "Beach Bowl Galaxy - Beachcombing for Purple Coins": SPRING,
    # Flying Star (Red Star)
    "Gateway Galaxy - Gateway's Purple Coins": FLYING,
}

OUT_OF_LOGIC = {
    "Gold Leaf Galaxy - Star Bunnies on the Hunt": "Bee Mushroom",
    "Gold Leaf Galaxy - When It Rains, It Pours": "Bee Mushroom",
    "Ghostly Galaxy - Luigi and the Haunted Mansion": "Boo Mushroom (a big out-of-logic trick)",
    "Deep Dark Galaxy - Bubble Blastoff": "Fire Flower and Ice Flower",
    "Beach Bowl Galaxy - Wall Jumping Up Waterfalls": "Ice Flower",
    "Toy Time Galaxy - Heavy Metal Mecha-Bowser": "Spring Mushroom",
    "Toy Time Galaxy - Bouncing Down Cake Lane": "Spring Mushroom",
}


def form_requirement(star_name: str):
    """The groups of alternatives the star asks for (empty when it asks for nothing)."""
    return FORM_STARS.get(star_name, ())
