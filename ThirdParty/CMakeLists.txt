
# assimp (includes zlibstatic)
if(ASSIMP_STATIC_LIB)
    set(BUILD_SHARED_LIBS OFF)
else()
    set(BUILD_SHARED_LIBS ON)
endif()
add_subdirectory(assimp)
set_property(TARGET assimp PROPERTY FOLDER "ThirdParty")

# Assimp include wrapper (assimp manages its own includes via generator expressions,
# so we use an INTERFACE library to expose the source include path to consumers)
add_library(assimp_includes INTERFACE)
target_include_directories(assimp_includes INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/assimp/include)
target_link_libraries(assimp_includes INTERFACE assimp)

# zlib headers (from assimp's internal build) as an INTERFACE target
add_library(zlib_headers INTERFACE)
target_include_directories(zlib_headers INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/assimp/contrib/zlib
    ${CMAKE_CURRENT_BINARY_DIR}/assimp/contrib/zlib
)


# Lithtech SDK (Windows only - MSVC)
if(WIN32)
    set(ENABLE_LITHTECH ON)
    add_subdirectory(lithtech)
else()
    set(ENABLE_LITHTECH OFF)
    message(STATUS "Lithtech SDK disabled on Linux")
endif()
set(ENABLE_LITHTECH ${ENABLE_LITHTECH} PARENT_SCOPE)

#Discord Social SDK (prebuilt)
add_library(discord_partner_sdk SHARED IMPORTED GLOBAL)
if(WIN32)
    set_target_properties(discord_partner_sdk PROPERTIES
        IMPORTED_IMPLIB "${CMAKE_CURRENT_SOURCE_DIR}/discord_social_sdk/lib/release/discord_partner_sdk.lib"
        IMPORTED_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/discord_social_sdk/bin/release/discord_partner_sdk.dll"
    )
else()
    set_target_properties(discord_partner_sdk PROPERTIES
        IMPORTED_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/discord_social_sdk/lib/release/libdiscord_partner_sdk.so"
        IMPORTED_NO_SONAME TRUE
    )
endif()
target_include_directories(discord_partner_sdk INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}/discord_social_sdk/include"
)

# lzma
add_subdirectory(lzma)

# Granny2 SDK (Windows only)
if(WIN32)
    set(GRANNY2_SDK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/granny2sdk")
    set(GRANNY2_SDK_INCLUDE_DIR "${GRANNY2_SDK_DIR}/include")
    set(GRANNY2_SDK_LIB_DIR "${GRANNY2_SDK_DIR}/lib")
    set(GRANNY2_SDK_LIB_PLATFORM_DIR "${GRANNY2_SDK_LIB_DIR}/win64")

    if(NOT EXISTS "${GRANNY2_SDK_INCLUDE_DIR}/granny.h")
        message(FATAL_ERROR "granny.h not found at ${GRANNY2_SDK_INCLUDE_DIR}/granny.h")
    endif()

    if(NOT EXISTS "${GRANNY2_SDK_LIB_PLATFORM_DIR}/granny2_x64.lib")
        message(FATAL_ERROR "Granny2 import library not found at ${GRANNY2_SDK_LIB_PLATFORM_DIR}/granny2_x64.lib")
    endif()

    add_library(granny2 SHARED IMPORTED GLOBAL)
    set_target_properties(granny2 PROPERTIES
        IMPORTED_IMPLIB "${GRANNY2_SDK_LIB_PLATFORM_DIR}/granny2_x64.lib"
        IMPORTED_LOCATION "${GRANNY2_SDK_LIB_PLATFORM_DIR}/granny2_x64.dll"
    )
    target_include_directories(granny2 INTERFACE "${GRANNY2_SDK_INCLUDE_DIR}")

    set(ENABLE_GRANNY2 ON)
    # Export the DLL path for install() commands in src/CMakeLists.txt
    set(GRANNY2_DLL_PATH "${GRANNY2_SDK_LIB_PLATFORM_DIR}/granny2_x64.dll" PARENT_SCOPE)
    set(GRANNY2_SDK_LIB_DIR "${GRANNY2_SDK_LIB_DIR}" PARENT_SCOPE)
    message(STATUS "Found Granny2 SDK: ${GRANNY2_SDK_LIB_PLATFORM_DIR}/granny2_x64.lib")
else()
    set(ENABLE_GRANNY2 OFF)
    message(STATUS "Granny2 SDK disabled on Linux (Windows-only)")
endif()
set(ENABLE_GRANNY2 ${ENABLE_GRANNY2} PARENT_SCOPE)

# gcl (header-only)
add_library(gcl_headers INTERFACE)
target_include_directories(gcl_headers INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/gcl")

# audio (header-only: dr_mp3, dr_wav, stb_vorbis) 
add_library(audio_headers INTERFACE)
target_include_directories(audio_headers INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/audio")

# libimagequant (color quantization - used by sprite conversion)
set(LIQ_DIR "${CMAKE_CURRENT_SOURCE_DIR}/libimagequant")
add_library(libimagequant STATIC
    ${LIQ_DIR}/libimagequant.c
    ${LIQ_DIR}/blur.c
    ${LIQ_DIR}/kmeans.c
    ${LIQ_DIR}/mediancut.c
    ${LIQ_DIR}/mempool.c
    ${LIQ_DIR}/nearest.c
    ${LIQ_DIR}/pam.c
    ${LIQ_DIR}/remap.c
)
target_include_directories(libimagequant PUBLIC ${LIQ_DIR})
set_target_properties(libimagequant PROPERTIES
    C_STANDARD 99
    POSITION_INDEPENDENT_CODE ON
    FOLDER "ThirdParty"
)