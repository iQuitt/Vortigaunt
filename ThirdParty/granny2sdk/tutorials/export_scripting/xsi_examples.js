// ========================================================================
// $File: //jeffr/granny/tutorial/export_scripting/xsi_examples.js $
// $DateTime: 2007/02/07 17:55:05 $
// $Change: 14382 $
// $Revision: #2 $
// 
// $Notice: $
// ========================================================================


/* DEFTUTORIAL EXPGROUP(TutorialScriptingXSI) (TutorialScriptingXSI_Motivation, Motivation) */
/* Scripting your export pipeline is a Very Good Idea.  In addition to making life easier
  for Artists, it makes life easier for the coders, by reducing variability in your art
  base, and eliminating a common source of (understandable!) errors.  We make sure that if
  you can do it through the Granny interface that it can be automated with a script in any
  tool that we support.
  $-

  We'll cover some simple scripts here that demonstrate a couple of common tasks that
  you'll need to master in order to create an automated pipeline, including object
  filtering and postprocessing exported files.  From here it's easy to imagine adding
  automated version control handling, etc.  Any thing you can automate, you absolutely
  should.  Better Living through Scripting!
  $-

  Don't forget to check out: $ExportingOverview_Scripting for more scripting
  documentation.  The functions below can be found in your tutorials directory, in
  "export_scripting/xsi_examples.js".
  $-

  XSI Specific Note: XSI supports a number of scripting engines, including VBScript,
  JScript, and Python.  For the purposes of this example, we'll use only JScript.  Though
  the Granny scripting bindings will work in any of the script engines, the Python binding
  requires external components as of XSI 6, so we don't want to count on it being present
  on your machine.
*/

/* DEFTUTORIAL EXPGROUP(TutorialScriptingXSI) (TutorialScriptingXSI_IgnoreHiddenChildren, IgnoreHiddenChildren) */
/* IgnoreHiddenChildren takes an XSI object as an argument, and loops through all the child
  nodes.  For each node, it checks to see if the node has been hidden by the artist, and
  if it has, it will instruct Granny to ignore the mesh, bone, and bone animation data for
  the node.  It calls itself recursively, so it processes children's children, and their
  children, and so on, until the entire tree origining from the initial parent has been
  processed.
  $-

  Since we're illustrating the use of the GrannySetValue command here, you'll want to
  look at $SetValueDocs_XSI for more information.  This is the most useful command in
  Granny's script bindings, so you should definitely be familiar with it.
*/
function IgnoreHiddenChildren( Parent )
{
    // Note that FindChilden by default traverses the entire object tree below the object,
    // so there's no need to recurse manually.
    var Children = Parent.FindChildren();
    for (var i = 0; i < Children.Count; ++i)
    {
        var thisChild = Children.Item(i);
        var IsVisible = thisChild.Properties("Visibility").viewvis.Value;

        if (!IsVisible)
        {
            GrannySetValue("include reference in export", 0, "bone", thisChild);
            GrannySetValue("include reference in export", 0, "boneanim", thisChild);
            GrannySetValue("include reference in export", 0, "mesh", thisChild);
            GrannySetValue("include reference in export", 0, "model", thisChild);
            GrannySetValue("include reference in export", 0, "modelanim", thisChild);
        }
    }
}

/* DEFTUTORIAL EXPGROUP(TutorialScriptingXSI) (TutorialScriptingXSI_IgnoreHiddenObject, IgnoreHiddenObjects) */
/*  IgnoreHiddenObjects works identically to IgnoreHiddenChildren, but for the entire scene
    instead of just the descendants of a particular node.
*/
function IgnoreHiddenObjects()
{
    IgnoreHiddenChildren(Application.ActiveSceneRoot);
}

/* DEFTUTORIAL EXPGROUP(TutorialScriptingXSI) (TutorialScriptingXSI_IgnoreAllTextures, IgnoreAllTextures) */
/* IgnoreAllTextures turns off texture exporting for the entire scene.  Note that unlike
  the Max and Maya bindings, the call to set global settings is a distinct function in
  XSI.
*/
function IgnoreAllTextures()
{
    GrannySetGlobalValue("include data in export", 0, "texture");
}

/* DEFTUTORIAL EXPGROUP(TutorialScriptingXSI) (TutorialScriptingXSI_BatchExport, Batch Export) */
/* Switching gears, BasicBatchExport demonstrates how to loop through all the files that
  match a given directory source, and export them to a destination directory.  You would
  call it like this:
*/

// BasicBatchExport("d:/source_art/", "d:/exported_art/");

/* All the calls in this function are standard XSI functions except the GrannyExport
  operation, which is a simple call that takes a string argument (the file to export to),
  and a couple of control parameters to ensure that the entire scene is exported, and the
  user isn't queried to change the settings.
*/
function BasicBatchExport(SourcePath, DestDir)
{
    // Ensure that the user has an opportunity to save the scene if necessary
    NewScene(null, true);

    var Files = FindFilesInFolder(SourcePath, ".*\\.scn", true, false);
    for (var i = 0; i < Files.length; ++i)
    {
        var thisFile = Files[i];
        var gr2File = Files[i].replace(/.scn/, ".gr2");

        OpenScene(SourcePath + thisFile, false, null);
        GrannyExport(DestDir + gr2File, false, false);
    }

    // Start with a clean slate
    NewScene(null, false);
}

/* DEFTUTORIAL EXPGROUP(TutorialScriptingXSI) (TutorialScriptingXSI_AdvancedBatchExport, Advanced Batch Export) */
/*  AdvancedBatchExport demonstrates how to augment a simple batch exporter like
  BasicBatchExport with extra features.  It takes two extra parameters: a Granny exporter
  settings file to use for each export, and a user-supplied executable name that will be
  run on each file after it is exported.  You would call it like this:
*/
// AdvancedBatchExport("d:/source_art/",
//                     "d:/exported_art",
//                     "d:/source_art/standard_settings.ges",
//                     "d:/tools/postprocess.exe")

/* In addition to the GrannyExport call, described previously, this function makes use of
  the GrannyLoadSettings call, which takes a single argument which is the settings file
  to load.  Although not demonstrated here, you can also make the corresponding call,
  GrannySaveSettings, to save to a .ges file rather than load from it.
*/
function AdvancedBatchExport(SourcePath, DestDir, Settings, Processor)
{
    // Ensure that the user has an opportunity to save the scene if necessary
    NewScene(null, true);

    var Files = FindFilesInFolder(SourcePath, ".*\\.scn", true, false);
    for (var i = 0; i < Files.length; ++i)
    {
        var thisFile = Files[i];
        var gr2File = Files[i].replace(/.scn/, ".gr2");

        OpenScene(SourcePath + thisFile, false, null);
        GrannyLoadSettings(Settings);
        GrannyExport(DestDir + gr2File, false, false);
        System(Processor + " " + DestDir + gr2File);
    }

    // Start with a clean slate
    NewScene(null, false);
}

/* DEFTUTORIALEND */
