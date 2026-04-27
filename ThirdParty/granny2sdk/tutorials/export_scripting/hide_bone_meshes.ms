fn HBM_IsBone item =
(
    return (classof item == BoneGeometry or
            classof item == Dummy or
            classof item == Biped_Object)
)

fn HideBoneMeshes =
(
    allBones = for item in $objects where HBM_IsBone item == true collect item
    for bone in allBones do
    (
        GrannySetValue "Include reference in export" false mesh:bone
    )
)
