@REM Build the database of strings from the two files
preprocessor ExtractStrings ..\..\media\gryphon.gr2 ..\..\media\pixo_run.gr2 -database database.gsd

@REM Remap the files to the given database
preprocessor RemapStrings ..\..\media\gryphon.gr2 -database database.gsd -output gryphon_remap.gr2
preprocessor RemapStrings ..\..\media\pixo_run.gr2 -database database.gsd -output pixo_run_remap.gr2

@REM Run the results through the compressor to minimize space on disk
preprocessor Compress database.gsd -output database.gsd
preprocessor Compress gryphon_remap.gr2 -output gryphon_remap.gr2
preprocessor Compress pixo_run_remap.gr2 -output pixo_run_remap.gr2
