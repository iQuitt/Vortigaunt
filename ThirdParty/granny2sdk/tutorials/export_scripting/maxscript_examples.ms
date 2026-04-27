-- ========================================================================
-- $File$
-- $DateTime$
-- $Change$
-- $Revision$
--
-- $Notice: $
-- ========================================================================


/* DEFTUTORIAL EXPGROUP(TutorialScriptingMax) (TutorialScriptingMax_Motivation, Motivation) */
/* Scripting your export pipeline is a Very Good Idea.  In addition to making life easier
   for Artists, it makes life easier for the coders, by reducing variability in your art
   base, and eliminating a common source of (understandable!) errors.  We make sure that
   if you can do it through the Granny interface that it can be automated with a script in
   any tool that we support.
   $-

   We'll cover some simple scripts here that demonstrate a couple of common tasks that
   you'll need to master in order to create an automated pipeline, including object
   filtering and postprocessing exported files.  From here it's easy to imagine adding
   automated version control handling, etc.  Any thing you can automate, you absolutely
   should.  Better Living through Scripting!
   $-

   Don't forget to check out: $ExportingOverview_Scripting for more scripting
   documentation.  The functions below can be found in your tutorials directory, in
   "export_scripting/maxscript_examples.ms".
 */

/* DEFTUTORIAL EXPGROUP(TutorialScriptingMax) (TutorialScriptingMax_IgnoreHiddenChildren, IgnoreHiddenChildren) */
/* IgnoreHiddenChildren takes a Max object as an argument, and loops through all the child
   nodes.  For each node, it checks to see if the node has been hidden by the artist, and
   if it has, it will instruct Granny to ignore the mesh, bone, and bone animation data
   for the node.  It calls itself recursively, so it processes children's children, and
   their children, and so on, until the entire tree origining from the initial parent has
   been processed.
   $-

   Since we're illustrating the use of the GrannySetValue command here, you'll want to
   look at $SetValueDocs_Max for more information.  This is the most useful command in
   Granny's script bindings, so you should definitely be familiar with it.
*/
fn IgnoreHiddenChildren Parent =
(
    for Child in Parent.children do
    (
        if Child.isHidden do
        (
            print ("Ignoring " + Child.name)
            GrannySetValue "include reference in export" 0 quiet:true bone:Child
            GrannySetValue "include reference in export" 0 quiet:true boneanim:Child
            GrannySetValue "include reference in export" 0 quiet:true mesh:Child
            GrannySetValue "include reference in export" 0 quiet:true model:Child
            GrannySetValue "include reference in export" 0 quiet:true modelanim:Child
        )

        IgnoreHiddenChildren Child
    )
)

/* DEFTUTORIAL EXPGROUP(TutorialScriptingMax) (TutorialScriptingMax_IgnoreHiddenObject, IgnoreHiddenObjects) */
/* IgnoreHiddenObjects works identically to IgnoreHiddenChildren, but for the entire scene
   instead of just on the descendants of a particular node.
*/
fn IgnoreHiddenObjects =
(
    IgnoreHiddenChildren rootNode
)

/* DEFTUTORIAL EXPGROUP(TutorialScriptingMax) (TutorialScriptingMax_IgnoreAllTextures, IgnoreAllTextures) */
/* IgnoreAllTextures turns off texture exporting for the entire scene. */
fn IgnoreAllTextures =
(
    GrannySetValue "include data in export" 0 global:"textures"
)

/* DEFTUTORIAL EXPGROUP(TutorialScriptingMax) (TutorialScriptingMax_ClearingSettings, Clearing Granny Settings) */
/* Granny settings for a scene can be cleared, so that they revert to their defaults.  You
   can do this on an entire scene like this:
*/
fn ClearAllSettings =
(
    GrannyClearAllSettings()
)

/* Or, you can clear just a specific object's Granny settings:
*/
fn ClearSettingsForObject Node =
(
    GrannyClearSettings Node
)

/* DEFTUTORIAL EXPGROUP(TutorialScriptingMax) (TutorialScriptingMax_BatchExport, Batch Export) */
/* Switching gears, BasicBatchExport demonstrates how to loop through all the files that
   match a given wildcard string, and export them to a destination directory.  You would
   call it like this:

   $-
   BasicBatchExport "d:/source_art/*.max" "d:/exported_art"
   $-

   All the calls in this function are standard MAXScript functions except the GrannyExport
   operation, which is a simple call that takes a single string argument (the file to
   export to).
*/
fn BasicBatchExport SourcePath DestDir =
(
    SourceFiles = getFiles SourcePath
    print ("Batch exporting wildcard " + SourcePath + " to " + DestDir)
    print ("Found " + (SourceFiles.count as string) + " file(s)")

    for SourceFile in SourceFiles do
    (
        DestFile = DestDir + "/" + getFilenameFile(SourceFile) + ".gr2"
        print ("Converting " + SourceFile + " to " + DestFile + "...")

        loadMaxFile SourceFile
        GrannyExport DestFile
    )
)

/* DEFTUTORIAL EXPGROUP(TutorialScriptingMax) (TutorialScriptingMax_AdvancedBatchExport, Advanced Batch Export) */
/* AdvancedBatchExport demonstrates how to augment a simple batch exporter like
   BasicBatchExport with extra features.  It takes two extra parameters: a Granny exporter
   settings file to use for each export, and a user-supplied executable name that will be
   run on each file after it is exported.  You would call it like this:

   $-
   AdvancedBatchExport "d:/source_art/*.max" "d:/exported_art" "d:/source_art/standard_settings.ges" "d:/tools/postprocess.exe"
   $-

   In addition to the GrannyExport call, described previously, this function makes use of
   the GrannyLoadSettings call, which takes a single argument which is the settings file
   to load.  Although not demonstrated here, you can also make the corresponding call,
   GrannySaveSettings, to save to a .ges file rather than load from it.
*/
fn AdvancedBatchExport SourcePath DestDir Settings PostProcess =
(
    SourceFiles = getFiles SourcePath
    print ("Batch exporting wildcard " + SourcePath + " to " + DestDir)
    print ("Using settings file " + Settings)
    print ("Using post-processor " + PostProcess)
    print ("Found " + (SourceFiles.count as string) + " file(s)")

    for SourceFile in SourceFiles do
    (
        DestFile = DestDir + "/" + getFilenameFile(SourceFile) + ".gr2"
        print ("Converting " + SourceFile + " to " + DestFile + "...")

        loadMaxFile SourceFile
        GrannyLoadSettings Settings
        GrannyExport DestFile
        DOSCommand (PostProcess + " " + DestFile)
    )
)

/* DEFTUTORIAL EXPGROUP(TutorialScriptingMax) (TutorialScriptingMax_MultipleAnim, Multiple Animation Export) */
/* MultiAnimExport demonstrates one way to export multiple animations
   from subsections of the larger timeline.  This script assumes the
   following: that the first note track on the scene node contains the
   sub-anim markers, and that the markers have the format: "name
   begin" "name end".  A sample file (multi_anim.max: Max 2009) is
   provided with the necessary track structure.  Open the "Dope Sheet"
   track view from the "Graph Editors" menu, and select the "Notes"
   track of the world node.  There are 5 animations, Ping1 through
   Ping5.
   $-
   Note that the example below does no settings management, which
   would be required in a production script.
*/
fn MultiAnimExport DestDir FilePrefix =
(
    origAnimRange = animationRange

    thescene = (refs.dependents rootnode)[1]
    if hasNoteTracks thescene do
    (
        notes = getNoteTrack thescene 1
        keys  = notes.keys

        for i = 1 to keys.count by 2 do
        (
            time_from = keys[i].time
            time_to   = keys[i+1].time

            name_str  = keys[i].value as stringstream
            anim_name = peekToken name_str

            full_path = DestDir + FilePrefix + "_" + anim_name + ".gr2"
            format "%: [%, %]\n" full_path time_from time_to

            animationRange = interval time_from time_to
            GrannyExport full_path

            DOSCommand ("preprocessor RenameElement " + full_path + " -output " + full_path + " -rename Animations[0] -newname " + anim_name)
            DOSCommand ("preprocessor Compress " + full_path + " -output " + full_path)
        )
    )

    animationRange = origAnimRange
)


/* DEFTUTORIAL EXPGROUP(TutorialScriptingMax) (TutorialScriptingMax_HideBoneMeshes, Hide bone helper meshes) */

/* Max attaches small helper meshes to bone objects to help you
   manipulate them in the interface.  Unfortunately, it can be very
   difficult for Granny to correctly distinguish between these helper
   meshes and actual user data.  This function will turn off the
   export of everything that <i>might</i> be a bone mesh.  If it works
   in your case as written, then excellent!  Otherwise you may need to
   add exceptions to handle oddball cases. */
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


/* DEFTUTORIALEND */
