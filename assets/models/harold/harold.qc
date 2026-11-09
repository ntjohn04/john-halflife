// John Half-Life's Harold (Blue Shift's ba_yard3, the wounded scientist): the Unified SDK's low-definition scientist
// (its modelsrc/models/scientist/scientist_ld.qc: body, needle, items, mouth, hitboxes, animations) with one head, the
// glasses scientist's made bald and without his glasses (John Half-Life's own). Built by scripts/build_models.py.

$modelname "harold.mdl"
$cd "./"

// The head's own textures first.
$cdtexture "./textures"
$include "../shared/includes/common/scientists/ld/common.qci"


//reference mesh(es)
$include "../shared/includes/bodygroups/scientists/ld/body.qci"

$bodygroup "heads"
{
studio "meshes/harold_head"
}

$include "../shared/includes/bodygroups/scientists/ld/needle.qci"
$include "../shared/includes/bodygroups/scientists/ld/items.qci"

$include "../shared/includes/texturegroups/scientists/ld/texturegroups.qci"

$include "../shared/includes/bonecontrollers/scientists/ld/bonecontrollers.qci"
$include "../shared/includes/bonecontrollers/scientists/ld/mouth.qci"

$include "../shared/includes/hitboxes/scientists/ld/hitboxes.qci"


$include "../shared/includes/animations/scientists/ld/animations.qci"
