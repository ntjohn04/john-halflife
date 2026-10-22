# MMod asset port

Some johnhalflife features use files from Half-Life: MMod (sounds, models, data tables). Those files belong to
MMod's authors, so they are **never committed here**. `port_assets.py` copies them from a local MMod
installation into the mod directory instead.

| File | Purpose |
|---|---|
| `assets.json` | The manifest: every MMod file used, with the feature that needs it and the SHA-256 of the MMod version it was taken from. Only listed files are ever installed. |
| `ASSETS.md` | Generated report of the manifest, grouped by folder and file type. This is the checklist to clear before publishing. |
| `port_assets.py` | Installs, verifies, removes and records assets. Python 3.9+, standard library only. |

The defaults point at the standard Steam paths for `Half-Life MMod\HL1MMod` and `Half-Life\johnhalflife`;
pass `--mmod-dir` and `--mod-dir` to override them.

```bat
python mmod_port\port_assets.py install            :: copy/convert everything in the manifest
python mmod_port\port_assets.py install --feature movement
python mmod_port\port_assets.py verify             :: state of each listed file in the mod directory
python mmod_port\port_assets.py remove             :: delete installed files that are unchanged
```

## Porting a new asset

Record it through the script rather than editing `assets.json`, so the hash comes from the actual file:

```bat
python mmod_port\port_assets.py add sound/weapons/foo.wav --feature weapons --notes "Replaces vanilla foo.wav"
python mmod_port\port_assets.py install --feature weapons
```

`add` also regenerates `ASSETS.md`. Commit both files with the code that uses the asset.

## Safety rules the script follows

- It refuses to overwrite a file in the mod directory that it didn't install. Use `--force` to override.
- It refuses to install a file whose hash differs from the manifest, for example after an MMod update. Re-run `add` to accept the new version.
- It keeps `mmod_port_installed.json` in the mod directory. `remove` uses it to delete only the files it installed.
