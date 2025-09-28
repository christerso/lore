# Audio System Library
add_library(lore_audio STATIC
    audio.cpp
)

target_include_directories(lore_audio
    PUBLIC
        ${CMAKE_SOURCE_DIR}/include
)

# Find and configure miniaudio
find_path(MINIAUDIO_INCLUDE_DIR miniaudio.h)
if(NOT MINIAUDIO_INCLUDE_DIR)
    # Download miniaudio if not found
    message(STATUS "Downloading miniaudio...")
    file(DOWNLOAD
        "https://raw.githubusercontent.com/mackron/miniaudio/master/miniaudio.h"
        "${CMAKE_BINARY_DIR}/external/miniaudio.h"
        SHOW_PROGRESS
    )
    set(MINIAUDIO_INCLUDE_DIR "${CMAKE_BINARY_DIR}/external")
endif()

target_include_directories(lore_audio
    PRIVATE
        ${MINIAUDIO_INCLUDE_DIR}
)

# Enable modern C++ features
target_compile_features(lore_audio
    PUBLIC
        cxx_std_20
)

# Audio-specific compiler optimizations
if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    # Only apply release optimizations in Release builds to avoid conflicts with debug flags
    target_compile_options(lore_audio PRIVATE
        $<$<CONFIG:Release>:/O2>        # Optimize for maximum speed in release only
        /arch:AVX2                      # Enable AVX2 instructions for DSP
        /fp:fast                        # Fast floating-point model for audio processing
        $<$<CONFIG:Release>:/GS->       # Disable buffer security checks for performance in release only
        $<$<CONFIG:Release>:/Oy>        # Omit frame pointers in release only
        /favor:INTEL64                  # Optimize for 64-bit Intel processors
    )
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(lore_audio PRIVATE
        -O3                    # Maximum optimization
        -march=native          # Optimize for target CPU
        -mavx2                 # Enable AVX2 instructions for DSP
        -msse4.2               # Enable SSE 4.2 instructions
        -mfma                  # Enable FMA instructions for faster math
        -ffast-math            # Fast math optimizations for audio processing
        -funroll-loops         # Unroll loops for performance
        -fomit-frame-pointer   # Omit frame pointers
        -ftree-vectorize       # Enable auto-vectorization
    )
endif()

# Audio-specific compiler definitions
target_compile_definitions(lore_audio
    PUBLIC
        LORE_AUDIO_3D_ENABLED           # Enable 3D spatial audio
        LORE_AUDIO_HEARING_SIMULATION   # Enable hearing damage simulation
        LORE_AUDIO_HRTF_ENABLED         # Enable HRTF processing
    PRIVATE
        _USE_MATH_DEFINES               # Enable M_PI and other math constants
        MA_ENABLE_ONLY_SPECIFIC_BACKENDS # Only enable specific miniaudio backends
        MA_ENABLE_WASAPI=1              # Enable WASAPI on Windows
        MA_ENABLE_DIRECTSOUND=1         # Enable DirectSound on Windows
        MA_ENABLE_ALSA=1                # Enable ALSA on Linux
        MA_ENABLE_PULSEAUDIO=1          # Enable PulseAudio on Linux
        MA_ENABLE_COREAUDIO=1           # Enable Core Audio on macOS
)

# Platform-specific audio libraries
if(WIN32)
    target_link_libraries(lore_audio
        PRIVATE
            winmm      # Windows Multimedia API
            ole32      # OLE32 for DirectSound
            user32     # User32 for window management
            advapi32   # Advanced API
    )
elseif(APPLE)
    find_library(COREAUDIO_FRAMEWORK CoreAudio)
    find_library(AUDIOTOOLBOX_FRAMEWORK AudioToolbox)
    find_library(AUDIOUNIT_FRAMEWORK AudioUnit)
    target_link_libraries(lore_audio
        PRIVATE
            ${COREAUDIO_FRAMEWORK}
            ${AUDIOTOOLBOX_FRAMEWORK}
            ${AUDIOUNIT_FRAMEWORK}
    )
elseif(UNIX)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(ALSA alsa)
    pkg_check_modules(PULSEAUDIO libpulse)

    if(ALSA_FOUND)
        target_link_libraries(lore_audio PRIVATE ${ALSA_LIBRARIES})
        target_include_directories(lore_audio PRIVATE ${ALSA_INCLUDE_DIRS})
    endif()

    if(PULSEAUDIO_FOUND)
        target_link_libraries(lore_audio PRIVATE ${PULSEAUDIO_LIBRARIES})
        target_include_directories(lore_audio PRIVATE ${PULSEAUDIO_INCLUDE_DIRS})
    endif()

    target_link_libraries(lore_audio PRIVATE pthread m dl)
endif()

# Link dependencies
target_link_libraries(lore_audio
    PUBLIC
        lore::ecs         # ECS system dependency
        lore::math        # Math system dependency
)

# Optional: Link with FFTW for advanced frequency analysis
option(LORE_USE_FFTW "Use FFTW library for advanced FFT operations" OFF)
if(LORE_USE_FFTW)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(FFTW3 REQUIRED fftw3f)
    target_link_libraries(lore_audio PUBLIC ${FFTW3_LIBRARIES})
    target_include_directories(lore_audio PUBLIC ${FFTW3_INCLUDE_DIRS})
    target_compile_definitions(lore_audio PUBLIC LORE_HAS_FFTW)
endif()

# Optional: Link with OpenAL for advanced 3D audio
option(LORE_USE_OPENAL "Use OpenAL for advanced 3D spatial audio" OFF)
if(LORE_USE_OPENAL)
    find_package(OpenAL QUIET)
    if(OPENAL_FOUND)
        target_link_libraries(lore_audio PUBLIC ${OPENAL_LIBRARY})
        target_include_directories(lore_audio PUBLIC ${OPENAL_INCLUDE_DIR})
        target_compile_definitions(lore_audio PUBLIC LORE_HAS_OPENAL)
    endif()
endif()

# Optional: Link with Steam Audio for advanced acoustics simulation
option(LORE_USE_STEAM_AUDIO "Use Steam Audio for advanced acoustic simulation" OFF)
if(LORE_USE_STEAM_AUDIO)
    # Steam Audio would need to be downloaded and configured separately
    target_compile_definitions(lore_audio PUBLIC LORE_HAS_STEAM_AUDIO)
endif()

# Set target properties
set_target_properties(lore_audio PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    POSITION_INDEPENDENT_CODE ON
)

# Enable comprehensive warnings
if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    target_compile_options(lore_audio PRIVATE
        /W4                    # High warning level
        /WX                    # Treat warnings as errors
        /wd4201                # Disable warning for nameless struct/union
        /wd4324                # Disable padding warnings for aligned structures
        /wd4244                # Disable conversion warnings for audio samples
        /wd4267                # Disable size_t conversion warnings
    )
else()
    target_compile_options(lore_audio PRIVATE
        -Wall -Wextra -Wpedantic
        -Werror                # Treat warnings as errors
        -Wno-unused-parameter  # Allow unused parameters in interface functions
        -Wno-padding           # Ignore padding warnings for aligned structures
        -Wno-conversion        # Allow implicit conversions for audio samples
    )
endif()

# Add alias for consistent naming
add_library(lore::audio ALIAS lore_audio)

# Optional: Enable audio profiling and debugging
option(LORE_AUDIO_DEBUG "Enable audio system debugging and profiling" OFF)
if(LORE_AUDIO_DEBUG)
    target_compile_definitions(lore_audio PUBLIC
        LORE_AUDIO_DEBUG_ENABLED
        LORE_AUDIO_PROFILING_ENABLED
    )
endif()

# Debug configuration specific settings
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_definitions(lore_audio PRIVATE
        LORE_AUDIO_DEBUG_ASSERTIONS     # Enable debug assertions
        LORE_AUDIO_DEBUG_LOGGING        # Enable debug logging
    )
endif()

# Release configuration specific settings
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    target_compile_definitions(lore_audio PRIVATE
        LORE_AUDIO_OPTIMIZED            # Enable release optimizations
        NDEBUG                          # Disable debug assertions
    )

    # Additional release optimizations for audio processing
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        target_compile_options(lore_audio PRIVATE
            -fno-math-errno          # Don't set errno on math functions
            -ffinite-math-only       # Assume finite math only
            -fno-trapping-math       # Don't trap on math exceptions
        )
    endif()
endif()