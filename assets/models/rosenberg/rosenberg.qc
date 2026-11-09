// John Half-Life's Rosenberg: the Unified SDK's (its modelsrc/models/rosenberg/rosenberg_ld.qc: its low-definition
// scientist's body, needle, items, mouth, hitboxes and animations) with a low-definition head, Paynamia's "Custom LD
// Rosenberg" (Improved LD Scientists + Civilians, https://gamebanana.com/mods/238622, CC BY-NC 4.0) in place of Blue
// Shift's, whose face was twice the detail of any other low-definition head. Built by scripts/build_models.py, which
// puts it in config/mod/models; the head came out of that mod's scientist.mdl by scripts/mdl2smd.py (README.md).

$modelname "rosenberg.mdl"
$cd "./"

// The head's own textures first: the scientists' share names with two of them (chrome, sci_mouth).
$cdtexture "./textures"
$include "../shared/includes/common/scientists/ld/common.qci"


//reference mesh(es)
$include "../shared/includes/bodygroups/scientists/ld/body.qci"

$bodygroup "heads"
{
studio "meshes/sci_ld_rosehead_roseglass"
}

$include "../shared/includes/bodygroups/scientists/ld/needle.qci"
$include "../shared/includes/bodygroups/scientists/ld/items.qci"

$include "../shared/includes/bonecontrollers/scientists/ld/bonecontrollers.qci"
$include "../shared/includes/bonecontrollers/scientists/ld/mouth.qci"

$include "../shared/includes/hitboxes/scientists/ld/hitboxes.qci"


$include "../shared/includes/animations/scientists/ld/animations.qci"
