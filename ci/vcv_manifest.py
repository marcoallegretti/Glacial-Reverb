#!/usr/bin/env python3
"""Stamp our identity onto the VCV Rack manifest erbb regenerates each build.

    python3 ci/vcv_manifest.py [--version 0.1.0] <plugin.json> ...
"""

import json
import sys

# "slug" is Rack's global plugin identifier and must be unique across every
# installed plugin -- it cannot be the brand, because 23DSP/EasyLive already
# claims the slug "23DSP" and Rack skips the second plugin that declares it.
# "brand" is the field the module browser groups by, so 23DSP modules still
# appear together. It also has to match this plugin's install directory name.
PLUGIN = {
    "slug": "KoloredVerb",
    "name": "Koloured-Verb",
    "brand": "23DSP",
    "author": "23DSP",
    "license": "EUPL-1.2",
    "authorUrl": "https://github.com/marcoallegretti",
    "pluginUrl": "https://github.com/marcoallegretti/Koloured-Verb",
    "manualUrl": "https://github.com/marcoallegretti/Koloured-Verb/blob/master/README.md",
    "sourceUrl": "https://github.com/marcoallegretti/Koloured-Verb",
}

MODULE = {
    "KoloredVerb": {
        "name": "Koloured-Verb",
        "description": (
            "Stereo reverb: 12 rooms in 6 families, 10 tail effects in "
            "5 families, freeze pad with ducking."
        ),
        "tags": ["Reverb", "Effect", "Digital"],
    },
}


def stamp(path, version=None):
    with open(path, encoding="utf-8") as f:
        manifest = json.load(f)

    manifest.update(PLUGIN)
    if version:
        manifest["version"] = version

    for module in manifest.get("modules", []):
        overrides = MODULE.get(module.get("slug"))
        if overrides:
            module.update(overrides)
        else:
            print("  warning: no override for module slug %r" % module.get("slug"))

    with open(path, "w", encoding="utf-8", newline="\n") as f:
        json.dump(manifest, f, indent=2)
        f.write("\n")

    slugs = ", ".join(m.get("slug", "?") for m in manifest.get("modules", []))
    print("stamped %s (plugin %s, modules: %s)" % (path, manifest["slug"], slugs))


if __name__ == "__main__":
    args = sys.argv[1:]
    version = None
    if "--version" in args:
        i = args.index("--version")
        version = args[i + 1].lstrip("v")
        del args[i:i + 2]
    if not args:
        sys.exit(__doc__)
    for arg in args:
        stamp(arg, version)
