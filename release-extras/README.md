Files attached to every release next to the installer: the YAML template and the PopTracker pack.
Regenerate them when the APWorld or the tracker changes:
- YAML: Archipelago's "Generate Template Options" with the APWorld installed (file "Super Mario Galaxy PC Port.yaml").
- PopTracker: `python tools/gen_poptracker_pack.py release-extras/SMG-PC-Port-PopTracker.zip`
