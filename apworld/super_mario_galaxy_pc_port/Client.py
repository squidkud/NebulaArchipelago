"""The Super Mario Galaxy PC Port client: a tracker for the multiworld, started from the Archipelago Launcher.

The game talks to the server through Nebula itself (archipelago.ini); this client only watches the same slot. It adds
the commands /tracker, /stars and /places to the text client, and, when the Universal Tracker is installed, uses its
window as well (the places that are in logic, drawn on a map of what you can reach).
"""
import asyncio
import typing

import Utils
from CommonClient import ClientCommandProcessor, CommonContext, get_base_parser, gui_enabled, logger, server_loop

from .Locations import every_star
from .TrackerData import format_places, format_summary, summarize

GAME_NAME = "Super Mario Galaxy PC Port"
CLIENT_NAME = "Super Mario Galaxy PC Port Client"

TRACKER_LOADED = False
try:
    from worlds.tracker.TrackerClient import TrackerGameContext as BaseContext  # the Universal Tracker

    TRACKER_LOADED = True
except ModuleNotFoundError:
    BaseContext = CommonContext


class SMGCommandProcessor(ClientCommandProcessor):
    def _cmd_tracker(self) -> None:
        """The star counter, the Grand Stars, and each galaxy: stars checked and whether its star count is met."""
        if isinstance(self.ctx, SMGContext):
            for line in format_summary(self.ctx.summary()):
                logger.info(line)

    def _cmd_stars(self, galaxy: str = "") -> None:
        """The stars of the galaxies whose name contains the text given ([x]: checked), e.g. /stars Good Egg"""
        if not isinstance(self.ctx, SMGContext):
            return
        wanted = galaxy.strip().lower()
        if not wanted:
            logger.info("Say part of a galaxy's name, for instance: /stars good egg")
            return
        known = self.ctx.server_locations
        by_stage = {row.stage: row for row in self.ctx.summary().rows if wanted in row.name.lower()}
        if not by_stage:
            logger.info(f"No galaxy of this multiworld has \"{galaxy}\" in its name.")
            return
        for star in every_star:
            if star.stage in by_stage and star.code in known:
                logger.info(f"  [{'x' if star.code in self.ctx.checked_locations else ' '}] {star.location_name}")

    def _cmd_places(self) -> None:
        """Which galaxy each miniature of the Observatory shows (when the galaxies are shuffled)."""
        if isinstance(self.ctx, SMGContext):
            for line in format_places(self.ctx.slot_data_smg):
                logger.info(line)


class SMGContext(BaseContext):
    game = GAME_NAME
    items_handling = 0b111  # the game's own connection applies the items; this one only counts them
    command_processor = SMGCommandProcessor
    slot_data_smg: typing.Optional[dict] = None

    if not TRACKER_LOADED:
        tags = CommonContext.tags | {"Tracker"}

    def on_package(self, cmd: str, args: dict) -> None:
        super().on_package(cmd, args)
        if cmd == "Connected":
            self.slot_data_smg = args.get("slot_data") or {}
            logger.info("Connected: /tracker shows your progress, /stars <galaxy> its stars, /places the shuffled galaxies.")

    def summary(self):
        return summarize(self.slot_data_smg, self.checked_locations, self.server_locations,
                         [item.item for item in self.items_received])


def launch(*launch_args: str) -> None:
    async def main(args) -> None:
        ctx = SMGContext(args.connect, args.password)
        ctx.auth = args.name
        ctx.server_task = asyncio.create_task(server_loop(ctx), name="server loop")
        if gui_enabled:
            ctx.run_gui()
        ctx.run_cli()
        await ctx.exit_event.wait()
        ctx.server_address = None
        await ctx.shutdown()

    import colorama

    Utils.init_logging(CLIENT_NAME)
    logger.info(f"Starting {CLIENT_NAME}" + (" with the Universal Tracker" if TRACKER_LOADED else ""))
    parser = get_base_parser(description="Tracker for Super Mario Galaxy PC Port.")
    args = parser.parse_args(launch_args)
    colorama.just_fix_windows_console()
    asyncio.run(main(args))
    colorama.deinit()
