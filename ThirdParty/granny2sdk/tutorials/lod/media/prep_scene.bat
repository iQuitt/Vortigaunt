preprocessor KillUncommonData ..\..\media\pixo_run.gr2 -output pixo_proc.gr2
preprocessor MeshIndices16Bit pixo_proc.gr2 -output pixo_proc.gr2
preprocessor VertexCacheOptimize pixo_proc.gr2 -output pixo_proc.gr2
preprocessor RemoveVectorTracks pixo_proc.gr2 -output pixo_proc.gr2

@REM    This is the key preprocessor call for Animation LOD.  In this
@REM    case, the mesh and skeleton reside in the same file as the
@REM    animation, so we specify it twice on the command line.
@REM
preprocessor LODCalc pixo_proc.gr2 pixo_proc.gr2 -output pixo_proc.gr2


@REM    If you had a setup where the mesh and skeleton were contained in
@REM    mesh.gr2, and your animations were stored in separate files, you
@REM    would instead do something like this:
@REM
@REM preprocessor LODCalc character.gr2 walk.gr2 -output walk_lod.gr2
@REM preprocessor LODCalc character.gr2 run.gr2  -output run_lod.gr2


preprocessor BonesPerMesh pixo_proc.gr2 -output pixo_proc.gr2 -bonelimit 64
preprocessor ResectionFile pixo_proc.gr2 -output pixo_proc.gr2
preprocessor Compress pixo_proc.gr2 -output pixo_final.gr2
