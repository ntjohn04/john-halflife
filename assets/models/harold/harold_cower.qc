// John Half-Life's Harold dying (ba_yard3 swaps him for this for his death): the Unified SDK's scientist_cower (its
// modelsrc/models/scientist/scientist_cower_ld.qc, its one sequence) with Harold's head. Built by
// scripts/build_models.py.

$modelname "harold_cower.mdl"
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


$include "../shared/includes/animations/scientists/ld/cower_die.qci"
