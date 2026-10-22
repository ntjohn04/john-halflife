# johnhalflife asset sources

Sources for assets this mod adds that are derived from the stock Half-Life games, not from MMod.
MMod files are never stored here; see `mmod_port/`.

| Asset | Source | Installed as | Made from |
|---|---|---|---|
| Bullet impact dust puff | `sprites/jhl_impact_puff/` | `sprites/jhl_impact_puff.spr` | Half-Life's `sprites/steam1.spr` |

## Sprites

Each sprite folder has one BMP per frame plus a `.qc` build file for `sprgen`.

Frames play in the order they appear in the `.qc`, one `$load` + `$frame` pair each. To drop frames, delete
those pairs (and optionally their BMPs). To change the art, edit the BMPs, keeping them 8-bit indexed with
the same palette.

Compile and install into the mod directory from the workspace root:

```powershell
.\scripts\build_sprite.ps1 -Qc sdk\halflife-unified-sdk\assetsrc\sprites\jhl_impact_puff\jhl_impact_puff.qc
```

To turn an existing sprite into editable frames, use `python re\tools\spr2qc.py <file.spr> <output folder> --name <name>`.
