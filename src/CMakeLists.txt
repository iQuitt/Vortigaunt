set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# MSE (Metin2 Script Effect) - disabled by default, enable with -DMETIN2_SCRIPT_EFFECT=ON
option(METIN2_SCRIPT_EFFECT "Enable Metin2 Script Effect support" OFF)

set(VORTIGAUNT_CORE_HEADERS
    core/converters/Gr2Converter.h
    core/WadMaker.h
    
    core/QCFile.h
    core/converters/AudioConvert.h
    utils/fsutils.hpp
    core/SpriteLoader.h
    core/extractors/xfs/XfsExtractor.h

    core/smd/SmdParser.h
    core/autorig/AutoRig.h
    
    core/extractors/rez/RezExtractor.h
    core/extractors/pak/SnowCipher.h
    core/extractors/pak/PakView.hpp
    core/extractors/pak/PakFile.h
    core/extractors/pak/PakExtractor.h
    utils/util.hpp
    utils/Dds.h
    core/VortigauntLog.h
    utils/Platform.h
    utils/Bmp.h
    core/extractors/IExtractor.h
    core/extractors/ExtractArchive.h
    core/extractors/unity/UnityAssetParser.h
    core/extractors/unity/UnityDataResolver.h
    core/extractors/unity/UnityFsExtractor.h
    core/extractors/unity/UnityMeshConverter.h
    core/extractors/unity/UnityPorter.h
    core/extractors/unity/UnityTextureDecoder.h
    core/extractors/vpk/VpkFile.h
    core/extractors/vpk/VpkExtractor.h
)

set(VORTIGAUNT_GUI_HEADERS

    ui/dialogs/DtxViewerDialog.h
    ui/dialogs/SpriteFixDialog.h
    ui/dialogs/SettingsDialog.h
    ui/dialogs/LoLModelDownloadDialog.h
    ui/dialogs/AutoRigDialog.h
    ui/dialogs/WadMakerDialog.h
    ui/dialogs/AboutDialog.h
    
    ui/dialogs/AudioConvertDialog.h

    ui/windows/PakViewerWindow.h
    ui/windows/RezViewerWindow.h
    ui/windows/SpriteViewerWindow.h
    ui/windows/XfsViewerWindow.h
    ui/windows/VpkViewerWindow.h


    ui/widgets/LoLModelDownloader.h
    ui/widgets/GLBViewer.h


    core/LanguageManager.h
    core/VortigauntUpdateCheck.h
)

set(VORTIGAUNT_CORE
    core/smd/SmdWriter.cpp
    core/WadMaker.cpp
    
    core/QCFile.cpp
    core/converters/AudioConvert.cpp
    core/SpriteLoader.cpp
    core/extractors/xfs/XfsExtractor.cpp

    core/smd/SmdParser.cpp
    core/autorig/AutoRig.cpp
    core/extractors/rez/RezExtractor.cpp
    utils/fsutils.cpp
    utils/Platform.cpp
    utils/Dds.cpp
    core/extractors/pak/SnowCipher.cpp
    core/extractors/pak/PakFile.cpp
    core/extractors/pak/PakExtractor.cpp
    core/extractors/ExtractArchive.cpp
    core/extractors/vpk/VpkFile.cpp
    core/extractors/vpk/VpkExtractor.cpp
    core/VortigauntLog.cpp
    core/extractors/unity/UnityAssetParser.cpp
    core/extractors/unity/UnityDataResolver.cpp
    core/extractors/unity/UnityFsExtractor.cpp
    core/extractors/unity/UnityMeshConverter.cpp
    core/extractors/unity/UnityPorter.cpp
    core/extractors/unity/UnityTextureDecoder.cpp
)

# Granny2 source files (Windows only)
if(ENABLE_GRANNY2)
    list(APPEND VORTIGAUNT_CORE
        core/converters/Gr2Converter.cpp
    )
endif()

# Lithtech source files (Windows only)
if(ENABLE_LITHTECH)
    list(APPEND VORTIGAUNT_CORE
        core/converters/LtbConverter.cpp
        core/converters/DtxConverter.cpp
        core/converters/LzmaDecoder.cpp
    )
    list(APPEND VORTIGAUNT_CORE_HEADERS
        core/converters/LtbConverter.h
        core/converters/DtxConverter.h
        core/converters/LzmaDecoder.h
    )
endif()

set(CLI_SOURCES
    core/main.cpp
)

set(VORTIGAUNT_GUI
    ui/main.cpp
    ui/MainWindow.cpp

    ui/dialogs/DtxViewerDialog.cpp
    ui/dialogs/SpriteFixDialog.cpp
    ui/dialogs/SettingsDialog.cpp
    ui/dialogs/LoLModelDownloadDialog.cpp
    ui/dialogs/AutoRigDialog.cpp
    ui/dialogs/WadMakerDialog.cpp
    ui/dialogs/AboutDialog.cpp
    
    ui/dialogs/AudioConvertDialog.cpp

    ui/windows/PakViewerWindow.cpp
    ui/windows/RezViewerWindow.cpp
    ui/windows/SpriteViewerWindow.cpp
    ui/windows/XfsViewerWindow.cpp
    ui/windows/VpkViewerWindow.cpp


    ui/widgets/LoLModelDownloader.cpp
    ui/widgets/GLBViewer.cpp


    core/LanguageManager.cpp
    core/VortigauntUpdateCheck.cpp
)

add_executable(VortigauntTool ${CLI_SOURCES} ${VORTIGAUNT_CORE_HEADERS} ${VORTIGAUNT_CORE})
target_include_directories(VortigauntTool PRIVATE 
    ./
    ./core
    ./core/converters
    ./ui
    ./utils
)
target_link_libraries(VortigauntTool PRIVATE
    assimp_includes zlib_headers lzma lz4
    gcl_headers audio_headers libimagequant
    unordered_dense::unordered_dense
)
if(ENABLE_LITHTECH)
    target_link_libraries(VortigauntTool PRIVATE lithtech)
    target_compile_definitions(VortigauntTool PRIVATE ENABLE_LITHTECH)
endif()
if(ENABLE_GRANNY2)
    target_link_libraries(VortigauntTool PRIVATE granny2)
    target_compile_definitions(VortigauntTool PRIVATE ENABLE_GRANNY2)
endif()
if(METIN2_SCRIPT_EFFECT)
    target_sources(VortigauntTool PRIVATE core/MseLoader.h core/MseLoader.cpp core/MdeLoader.h core/MdeLoader.cpp)
    target_compile_definitions(VortigauntTool PRIVATE METIN2_SCRIPT_EFFECT)
endif()




if(BUILD_GUI)
    set(QT_RESOURCE_FILE "${CMAKE_CURRENT_SOURCE_DIR}/resources/resources.qrc")

    if(WIN32)
        set(PLATFORM_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/resources/app.rc")
    else()
        set(PLATFORM_SOURCES "")# didnt tested so im not sure about that
    endif()

    add_executable(VortigauntTool_gui WIN32
        ${VORTIGAUNT_GUI}
        ${VORTIGAUNT_GUI_HEADERS}
        ${VORTIGAUNT_CORE_HEADERS}
        ${VORTIGAUNT_CORE}
        ${PLATFORM_SOURCES}
        ${QT_RESOURCE_FILE}
     "core/Discord_Integration.cpp")
    target_include_directories(VortigauntTool_gui PRIVATE 
        ./
        ./core
        ./core/converters
        ./ui
        ./ui/dialogs
        ./ui/windows
        ./ui/widgets
        ./utils
    )
    target_link_libraries(VortigauntTool_gui PRIVATE
        assimp_includes zlib_headers lzma lz4 discord_partner_sdk
        gcl_headers audio_headers libimagequant
        Qt6::Widgets Qt6::Network Qt6::OpenGL Qt6::OpenGLWidgets Qt6::Multimedia
        unordered_dense::unordered_dense
    )
    if(ENABLE_LITHTECH)
        target_link_libraries(VortigauntTool_gui PRIVATE lithtech)
        target_compile_definitions(VortigauntTool_gui PRIVATE ENABLE_LITHTECH)
    endif()
    if(ENABLE_GRANNY2)
        target_link_libraries(VortigauntTool_gui PRIVATE granny2)
        target_compile_definitions(VortigauntTool_gui PRIVATE ENABLE_GRANNY2)
    endif()
    if(METIN2_SCRIPT_EFFECT)
        target_sources(VortigauntTool_gui PRIVATE
            core/MseLoader.h core/MseLoader.cpp
            core/MdeLoader.h core/MdeLoader.cpp
            ui/windows/MseViewerWindow.h ui/windows/MseViewerWindow.cpp
            ui/widgets/MseViewerWidget.h ui/widgets/MseViewerWidget.cpp
        )
        target_compile_definitions(VortigauntTool_gui PRIVATE METIN2_SCRIPT_EFFECT)
    endif()

    # Qt Translation files
    set(TS_FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/resources/translations/vortigaunt_en.ts
        ${CMAKE_CURRENT_SOURCE_DIR}/resources/translations/vortigaunt_tr.ts
        ${CMAKE_CURRENT_SOURCE_DIR}/resources/translations/vortigaunt_es.ts
        ${CMAKE_CURRENT_SOURCE_DIR}/resources/translations/vortigaunt_pl.ts
    )

    # Create translations directory if it doesn't exist
    file(MAKE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/resources/translations)

    # Add translations - this will create lupdate and lrelease targets
    qt_add_translations(VortigauntTool_gui
        TS_FILES ${TS_FILES}
        QM_FILES_OUTPUT_VARIABLE QM_FILES
        LUPDATE_OPTIONS -no-obsolete -locations relative
        LRELEASE_OPTIONS -compress
    )

    # Copy .qm files to build directory
    add_custom_command(TARGET VortigauntTool_gui POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:VortigauntTool_gui>/translations"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${QM_FILES} "$<TARGET_FILE_DIR:VortigauntTool_gui>/translations"
        COMMENT "Copying translation files to output directory"
    )

    # Copy Discord Social SDK DLL to build directory
    add_custom_command(TARGET VortigauntTool_gui POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:discord_partner_sdk>"
            "$<TARGET_FILE_DIR:VortigauntTool_gui>"
        COMMENT "Copying discord_partner_sdk.dll to output directory"
    )

    # Install path
    set(CMAKE_INSTALL_PREFIX "${CMAKE_SOURCE_DIR}/install" CACHE PATH "Install path" FORCE)

    install(TARGETS VortigauntTool_gui
        RUNTIME DESTINATION .
    )

    install(FILES ${QM_FILES} DESTINATION translations)

    # Install Discord SDK DLL
    install(FILES "$<TARGET_FILE:discord_partner_sdk>" DESTINATION .)

    # --- Shell Integrations (DTX Thumbnail Provider) ---
    if(WIN32)
        # Windows Shell Extension COM DLL
        set(DTX_THUMBNAIL_PROVIDER_SOURCES
            shell/windows/DtxThumbnailProvider.cpp
            shell/windows/DtxThumbnailProvider.h
            shell/windows/DtxThumbnailProvider.def
            core/converters/DtxConverter.cpp
            core/converters/LzmaDecoder.cpp
        )
        add_library(DtxThumbnailProvider SHARED ${DTX_THUMBNAIL_PROVIDER_SOURCES})
        target_include_directories(DtxThumbnailProvider PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}
            ${CMAKE_CURRENT_SOURCE_DIR}/core
            ${CMAKE_CURRENT_SOURCE_DIR}/core/converters
            ${CMAKE_CURRENT_SOURCE_DIR}/utils
        )
        target_link_libraries(DtxThumbnailProvider PRIVATE
            assimp_includes zlib_headers lzma lz4 libimagequant
            unordered_dense::unordered_dense
            shlwapi
        )
        if(ENABLE_LITHTECH)
            target_link_libraries(DtxThumbnailProvider PRIVATE lithtech)
            target_compile_definitions(DtxThumbnailProvider PRIVATE ENABLE_LITHTECH)
        endif()
        
        # Specify DEF file for exports to avoid mangling and export standard COM DLL entrypoints
        set_target_properties(DtxThumbnailProvider PROPERTIES
            LINK_FLAGS "/DEF:\"${CMAKE_CURRENT_SOURCE_DIR}/shell/windows/DtxThumbnailProvider.def\""
        )
        
        # Install target
        install(TARGETS DtxThumbnailProvider DESTINATION .)
    else()
        # Linux / Unix desktop thumbnailer CLI is disabled because Lithtech SDK is Windows-only.
    endif()

    if(WIN32)
        # Find windeployqt
        get_target_property(_qmake_executable Qt6::qmake IMPORTED_LOCATION)
        get_filename_component(_qt_bin_dir "${_qmake_executable}" DIRECTORY)
        find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${_qt_bin_dir}")
        
        if(WINDEPLOYQT_EXECUTABLE)
            install(CODE "
                set(_INSTALL_CONFIG \"\${CMAKE_INSTALL_CONFIG_NAME}\")
                if(NOT _INSTALL_CONFIG)
                    set(_INSTALL_CONFIG \"${CMAKE_BUILD_TYPE}\")
                endif()
                
                if(_INSTALL_CONFIG STREQUAL \"Debug\")
                    set(_DEPLOY_FLAGS \"--debug\")
                else()
                    set(_DEPLOY_FLAGS \"--release\")
                endif()
                
                message(STATUS \"Running windeployqt with flags: \${_DEPLOY_FLAGS}\")
                execute_process(
                    COMMAND \"${WINDEPLOYQT_EXECUTABLE}\" 
                        \${_DEPLOY_FLAGS}
                        --no-translations
                        \"\${CMAKE_INSTALL_PREFIX}/VortigauntTool_gui.exe\"
                    RESULT_VARIABLE _deploy_result
                )
                if(NOT _deploy_result EQUAL 0)
                    message(WARNING \"windeployqt failed with result: \${_deploy_result}\")
                endif()
            ")
        endif()
        
        # Install Granny2 DLL for GUI (Windows only)
        if(ENABLE_GRANNY2)
            install(FILES "${GRANNY2_DLL_PATH}"
                DESTINATION .
            )
        endif()
    endif()
endif()

# Install CLI tool (always available)
install(TARGETS VortigauntTool
    RUNTIME DESTINATION .
)

# Copy Granny2 DLL for CLI too
if(WIN32 AND ENABLE_GRANNY2)
    install(FILES "${GRANNY2_DLL_PATH}"
        DESTINATION .
    )
endif()

# Linux Desktop Integration 
if(UNIX AND NOT APPLE)
    # Install .desktop file for application menu
    install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/resources/vortigaunt.desktop"
        DESTINATION share/applications
        RENAME vortigaunt-tool.desktop
    )
    
    # Install MIME type definitions for file associations
    install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/resources/vortigaunt-mimetypes.xml"
        DESTINATION share/mime/packages
        RENAME vortigaunt-tool.xml
    )
    
    # Install application icon
    install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/resources/app2.png"
        DESTINATION share/icons/hicolor/256x256/apps
        RENAME vortigaunt.png
    )
    
    # Post-install: Update MIME database and desktop database
    # This is typically handled by package managers, but we provide a helper script
    install(CODE "
        message(STATUS \"Updating MIME database...\")
        execute_process(COMMAND update-mime-database \${CMAKE_INSTALL_PREFIX}/share/mime
                        ERROR_QUIET)
        message(STATUS \"Updating desktop database...\")
        execute_process(COMMAND update-desktop-database \${CMAKE_INSTALL_PREFIX}/share/applications
                        ERROR_QUIET)
    ")
endif()

# CPack WiX MSI Installer Configuration

set(CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH};C:/Program Files (x86)/WiX Toolset v3.14/bin")
list(APPEND CMAKE_PREFIX_PATH "C:/Program Files (x86)/WiX Toolset v3.14")
set(CPACK_WIX_ROOT "C:/Program Files (x86)/WiX Toolset v3.14")

set(CPACK_GENERATOR "WIX")
set(CPACK_PACKAGE_NAME "VortigauntTool")
set(CPACK_PACKAGE_VENDOR "Vortigaunt")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "A Porting Tool for GoldSrc Engine")
set(CPACK_PACKAGE_VERSION "${VORTIGAUNT_VERSION_MAJOR}.${VORTIGAUNT_VERSION_MINOR}.${VORTIGAUNT_VERSION_PATCH}")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "VortigauntTool")
set(CPACK_PACKAGE_EXECUTABLES "VortigauntTool_gui" "Vortigaunt")

# Exclude assimp dev components
set(CPACK_COMPONENTS_ALL Unspecified)
set(CPACK_MONOLITHIC_INSTALL ON)

# WiX settings
set(CPACK_WIX_UPGRADE_GUID "F5759CCD-F2B7-46E7-AEA1-EBFB05763A46")
set(CPACK_WIX_PRODUCT_ICON "${CMAKE_CURRENT_SOURCE_DIR}/resources/app2.ico")
set(CPACK_WIX_PROGRAM_MENU_FOLDER "Vortigaunt")
set(CPACK_WIX_PROPERTY_ARPURLINFOABOUT "https://github.com/iQuitt/Vortigaunt")

# File associations for WiX
set(WIX_FILE_ASSOCIATIONS_CONTENT "<?xml version='1.0' encoding='UTF-8'?>
<Wix xmlns='http://schemas.microsoft.com/wix/2006/wi'>
  <Fragment>
    <DirectoryRef Id='TARGETDIR'>
      <Component Id='FileAssociations' Guid='A1B2C3D4-E5F6-7890-ABCD-EF1234567890'>
        <!-- REZ -->
        <ProgId Id='Vortigaunt.rez' Description='Lithtech REZ Archive' Icon='VortigauntTool_gui.exe'>
          <Extension Id='rez' ContentType='application/x-rez'>
            <Verb Id='open' Command='Open' TargetFile='VortigauntTool_gui.exe' Argument='\"%1\"'/>
          </Extension>
        </ProgId>
        <!-- DTX -->
        <ProgId Id='Vortigaunt.dtx' Description='Lithtech Texture File' Icon='VortigauntTool_gui.exe'>
          <Extension Id='dtx' ContentType='application/x-dtx'>
            <Verb Id='open' Command='Open' TargetFile='VortigauntTool_gui.exe' Argument='\"%1\"'/>
          </Extension>
        </ProgId>
        <!-- SPR -->
        <ProgId Id='Vortigaunt.spr' Description='Sprite File' Icon='VortigauntTool_gui.exe'>
          <Extension Id='spr' ContentType='application/x-sprite'>
            <Verb Id='open' Command='Open' TargetFile='VortigauntTool_gui.exe' Argument='\"%1\"'/>
          </Extension>
        </ProgId>
        <!-- MSE - disabled until METIN2_SCRIPT_EFFECT is enabled
        <ProgId Id='Vortigaunt.mse' Description='Metin2 Script Effect File' Icon='VortigauntTool_gui.exe'>
          <Extension Id='mse' ContentType='application/x-mse'>
            <Verb Id='open' Command='Open' TargetFile='VortigauntTool_gui.exe' Argument='\"%1\"'/>
          </Extension>
        </ProgId>
        -->
        <!-- XFS -->
        <ProgId Id='Vortigaunt.xfs' Description='Xenesis XFS Archive' Icon='VortigauntTool_gui.exe'>
          <Extension Id='xfs' ContentType='application/x-xfs'>
            <Verb Id='open' Command='Open' TargetFile='VortigauntTool_gui.exe' Argument='\"%1\"'/>
          </Extension>
        </ProgId>
        <!-- WAD -->
        <ProgId Id='Vortigaunt.wad' Description='Half-Life WAD Texture Pack' Icon='VortigauntTool_gui.exe'>
          <Extension Id='wad' ContentType='application/x-wad'>
            <Verb Id='open' Command='Open' TargetFile='VortigauntTool_gui.exe' Argument='\"%1\"'/>
          </Extension>
        </ProgId>
        <!-- PAK -->
        <ProgId Id='Vortigaunt.pak' Description='Counter Strike Online PAK Archive' Icon='VortigauntTool_gui.exe'>
          <Extension Id='pak' ContentType='application/x-pak'>
            <Verb Id='open' Command='Open' TargetFile='VortigauntTool_gui.exe' Argument='\"%1\"'/>
          </Extension>
        </ProgId>
        <!-- VPK -->
        <ProgId Id='Vortigaunt.vpk' Description='Source Engine VPK Archive' Icon='VortigauntTool_gui.exe'>
          <Extension Id='vpk' ContentType='application/x-vpk'>
            <Verb Id='open' Command='Open' TargetFile='VortigauntTool_gui.exe' Argument='\"%1\"'/>
          </Extension>
        </ProgId>
      </Component>
    </DirectoryRef>
  </Fragment>
  <Fragment>
    <DirectoryRef Id='DesktopFolder'>
      <Component Id='DesktopShortcutComponent' Guid='D1E2F3A4-B5C6-7890-EF01-23456789ABCD'>
        <Condition>INSTALLDESKTOPSHORTCUT</Condition>
        <Shortcut Id='DesktopShortcut'
                  Name='Vortigaunt'
                  Target='[INSTALL_ROOT]VortigauntTool_gui.exe'
                  WorkingDirectory='INSTALL_ROOT' />
        <RegistryValue Root='HKCU' Key='Software\\Vortigaunt' Name='DesktopShortcut' Type='integer' Value='1' KeyPath='yes' />
      </Component>
    </DirectoryRef>
  </Fragment>
  <Fragment>
    <ComponentGroup Id='FileAssociationsGroup'>
      <ComponentRef Id='FileAssociations'/>
      <ComponentRef Id='DesktopShortcutComponent'/>
    </ComponentGroup>
  </Fragment>
</Wix>")

file(WRITE "${CMAKE_BINARY_DIR}/wix_file_associations.wxs" "${WIX_FILE_ASSOCIATIONS_CONTENT}")

set(WIX_UI_CONTENT "<?xml version='1.0' encoding='UTF-8'?>
<Wix xmlns='http://schemas.microsoft.com/wix/2006/wi'>
  <Fragment>
    <UI Id='WixUI_InstallDir_NoEULA'>
      <UIRef Id='WixUI_InstallDir' />
      <Publish Dialog='WelcomeDlg' Control='Next' Event='NewDialog' Value='InstallDirDlg' Order='2'>1</Publish>
      <Publish Dialog='InstallDirDlg' Control='Back' Event='NewDialog' Value='WelcomeDlg' Order='2'>1</Publish>
      
      <Publish Dialog='InstallDirDlg' Control='Next' Event='NewDialog' Value='ShortcutsDlg' Order='5'>WIXUI_DONTVALIDATEPATH OR WIXUI_INSTALLDIR_VALID=\"1\"</Publish>
      <Publish Dialog='ShortcutsDlg' Control='Back' Event='NewDialog' Value='InstallDirDlg'>1</Publish>
      <Publish Dialog='ShortcutsDlg' Control='Next' Event='NewDialog' Value='VerifyReadyDlg'>1</Publish>
      <Publish Dialog='VerifyReadyDlg' Control='Back' Event='NewDialog' Value='ShortcutsDlg' Order='5'>1</Publish>

      <Publish Dialog='ExitDialog' Control='Finish' Event='DoAction' Value='LaunchApplication'>WIXUI_EXITDIALOGOPTIONALCHECKBOX = 1 and NOT Installed</Publish>

      <CustomActionRef Id='LaunchApplication' />
    </UI>
    
    <Property Id='INSTALLDESKTOPSHORTCUT' Value='1' />
    <Property Id='WIXUI_EXITDIALOGOPTIONALCHECKBOXTEXT' Value='Launch Vortigaunt' />
    <Property Id='WIXUI_EXITDIALOGOPTIONALCHECKBOX' Value='1' />
    
    <CustomAction Id='LaunchApplication' Directory='INSTALL_ROOT' ExeCommand='&quot;[INSTALL_ROOT]VortigauntTool_gui.exe&quot;' Execute='immediate' Impersonate='yes' Return='asyncNoWait' />

    <Dialog Id='ShortcutsDlg' Width='370' Height='270' Title='!(loc.InstallDirDlg_Title)'>
      <Control Id='Next' Type='PushButton' X='236' Y='243' Width='56' Height='17' Default='yes' Text='!(loc.WixUINext)' />
      <Control Id='Back' Type='PushButton' X='180' Y='243' Width='56' Height='17' Text='!(loc.WixUIBack)' />
      <Control Id='Cancel' Type='PushButton' X='304' Y='243' Width='56' Height='17' Cancel='yes' Text='!(loc.WixUICancel)'>
        <Publish Event='SpawnDialog' Value='CancelDlg'>1</Publish>
      </Control>
      
      <Control Id='Description' Type='Text' X='25' Y='23' Width='280' Height='15' Transparent='yes' NoPrefix='yes' Text='Select additional tasks to perform during installation.' />
      <Control Id='Title' Type='Text' X='15' Y='6' Width='200' Height='15' Transparent='yes' NoPrefix='yes' Text='{\\WixUI_Font_Title}Select Additional Tasks' />
      <Control Id='BannerLine' Type='Line' X='0' Y='44' Width='370' Height='0' />
      <Control Id='BottomLine' Type='Line' X='0' Y='234' Width='370' Height='0' />
      <Control Id='BannerBitmap' Type='Bitmap' X='0' Y='0' Width='370' Height='44' TabSkip='no' Text='!(loc.InstallDirDlgBannerBitmap)' />
      
      <Control Id='DesktopShortcutCheckBox' Type='CheckBox' X='25' Y='70' Width='320' Height='18' Property='INSTALLDESKTOPSHORTCUT' CheckBoxValue='1' Text='Create a desktop shortcut' />
    </Dialog>
  </Fragment>
</Wix>")

file(WRITE "${CMAKE_BINARY_DIR}/wix_ui.wxs" "${WIX_UI_CONTENT}")

set(CPACK_WIX_EXTRA_SOURCES "${CMAKE_BINARY_DIR}/wix_file_associations.wxs" "${CMAKE_BINARY_DIR}/wix_ui.wxs")
set(CPACK_WIX_FEATURE_GROUPS "FileAssociationsGroup")
set(CPACK_WIX_UI_REF "WixUI_InstallDir_NoEULA")

include(CPack)
