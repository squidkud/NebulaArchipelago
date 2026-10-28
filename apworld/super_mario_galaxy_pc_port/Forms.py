"""The stars that cannot be done without a transformation (used when the "Transformations as Items" option is on).

star name -> the transformation items of which the player needs at least one (a tuple of alternatives).

This is written from how the game's missions are built and covers the stars whose whole mission is the transformation;
it is not exhaustive (a star where the power-up only helps is not listed), and the six items are placed early anyway, so a
missing entry never makes a seed impossible: it only means the logic does not ask for the item.
"""
from .Items import BEE_MUSHROOM, BOO_MUSHROOM, FIRE_FLOWER, FLYING_STAR, ICE_FLOWER, SPRING_MUSHROOM

FORM_STARS = {
    "Honeyhive Galaxy - Bee Mario Takes Flight": (BEE_MUSHROOM,),
    "Honeyhive Galaxy - Trouble on the Tower": (BEE_MUSHROOM,),
    "Honeyhive Galaxy - Big Bad Bugaboom": (BEE_MUSHROOM,),
    "Honeyhive Galaxy - The Honeyhive's Purple Coins": (BEE_MUSHROOM,),
    "Honeyhive Galaxy - Luigi in the Honeyhive Kingdom": (BEE_MUSHROOM,),
    "Honeyclimb Galaxy - Scaling the Sticky Wall": (BEE_MUSHROOM,),
    "Boo's Boneyard Galaxy - Racing the Spooky Speedster": (BOO_MUSHROOM,),
    "Dusty Dune Galaxy - Soaring on the Desert Winds": (FLYING_STAR,),
    "Toy Time Galaxy - Bouncing Down Cake Lane": (SPRING_MUSHROOM,),
    "Freezeflame Galaxy - The Frozen Peak of Baron Brr": (FIRE_FLOWER, ICE_FLOWER),
    "Freezeflame Galaxy - Freezeflame's Blistering Core": (FIRE_FLOWER, ICE_FLOWER),
    "Freezeflame Galaxy - Hot and Cold Collide": (FIRE_FLOWER, ICE_FLOWER),
}
