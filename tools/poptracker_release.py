#!/usr/bin/env python3
"""Builds the PopTracker pack of a release and the versions.json PopTracker reads to find updates.

    python tools/poptracker_release.py v1.0.3 outdir [changelog line ...]

Writes outdir/SMG-PC-Port-PopTracker.zip (its package_version is the tag without the "v") and outdir/versions.json
(package_version, download_url, sha256, changelog). Both go on the release; the pack's manifest points PopTracker at
the latest release's versions.json, so a pack started from PopTracker is offered the update.
"""
import hashlib
import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import gen_poptracker_pack as pack

REPOSITORY = "squidkud/NebulaArchipelago"
ZIP_NAME = "SMG-PC-Port-PopTracker.zip"


def main(argv):
    if len(argv) < 3:
        sys.exit(__doc__)
    tag, outdir = argv[1], Path(argv[2])
    changelog = argv[3:] or [f"Nebula Archipelago {tag}"]
    version = tag[1:] if tag.startswith("v") else tag
    outdir.mkdir(parents=True, exist_ok=True)
    target = outdir / ZIP_NAME
    pack.build(str(target), version)
    digest = hashlib.sha256(target.read_bytes()).hexdigest()
    versions = {"versions": [{
        "package_version": version,
        "download_url": f"https://github.com/{REPOSITORY}/releases/download/{tag}/{ZIP_NAME}",
        "sha256": digest,
        "changelog": changelog,
    }]}
    (outdir / "versions.json").write_text(json.dumps(versions, indent=2), encoding="utf-8")
    print(f"wrote {target} ({version}) and versions.json (sha256 {digest})")


if __name__ == "__main__":
    main(sys.argv)
