# Third-Party Library Management

## Policy

**CRITICAL:** Once third-party libraries are downloaded and built, we use their pre-compiled binaries/libraries. We do NOT re-download or rebuild them unless explicitly needed.

This policy ensures:
- ✅ Faster build times
- ✅ Version stability
- ✅ Consistent builds across machines
- ✅ Reduced git repository size

## Directory Structure

```
lore/
├── third_party/          # Third-party source code (tracked in git)
│   ├── googletest/       # Keep source, ignore builds
│   ├── glm/             # Header-only, fully tracked
│   └── ...
│
└── external/            # Pre-built binaries (partially tracked)
    ├── include/         # Headers (tracked)
    ├── lib/            # Pre-built libs (IGNORED)
    └── bin/            # Pre-built DLLs (IGNORED)
```

## What's Tracked in Git

### ✅ **Always Tracked:**
- Third-party **source code** (`third_party/*/src/`)
- Third-party **headers** (`third_party/*/include/`, `external/include/`)
- Build scripts (`CMakeLists.txt`, `*.cmake`)
- Documentation (`README.md`, `LICENSE`)

### ❌ **Never Tracked:**
- Compiled libraries (`*.lib`, `*.a`, `*.so`, `*.dylib`)
- Executables (`*.exe`, `*.dll`)
- Build artifacts (`build/`, `bin/`, `lib/`, `CMakeFiles/`)
- IDE files (`.vs/`, `.vscode/`)

## Workflow

### Initial Setup (New Developer)

```bash
# 1. Clone repository
git clone https://github.com/christerso/lore.git
cd lore

# 2. Build third-party libraries (one-time)
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release

# 3. Use pre-built libraries forever after
# No need to rebuild unless:
# - Updating library version
# - Switching build configuration (Debug/Release)
# - Changing compiler/platform
```

### Using Pre-Built Libraries

```cmake
# CMakeLists.txt
# Link against pre-built third-party library
target_link_libraries(lore PRIVATE
    ${CMAKE_BINARY_DIR}/third_party/googletest/lib/gtest.lib
)
```

**DO NOT:**
- Run `FetchContent_MakeAvailable()` on every build
- Add `add_subdirectory(third_party/*)` unless necessary
- Rebuild libraries that haven't changed

**DO:**
- Use `find_package()` to locate pre-built libraries
- Use `find_library()` to link against pre-built binaries
- Cache library locations in CMake cache

## Current Third-Party Libraries

### 1. **GoogleTest** (Unit Testing)
- **Location:** `third_party/googletest/`
- **Usage:** Link against `gtest.lib`, `gtest_main.lib`
- **Build Once:** Yes, use pre-built binaries

### 2. **GLM** (Math Library)
- **Location:** `third_party/glm/`
- **Usage:** Header-only, no build required
- **Build Once:** N/A (headers only)

### 3. **Vulkan SDK** (Graphics API)
- **Location:** System-wide installation
- **Usage:** `find_package(Vulkan REQUIRED)`
- **Build Once:** N/A (system library)

### 4. **VMA** (Vulkan Memory Allocator)
- **Location:** `third_party/vma/` or `external/vma/`
- **Usage:** Header-only (`vk_mem_alloc.h`)
- **Build Once:** N/A (single header)

### 5. **miniaudio** (Audio Library)
- **Location:** `external/miniaudio.h`
- **Usage:** Single-header library
- **Build Once:** N/A (header-only, compiles with project)

### 6. **ImGui** (UI Library) - If Added
- **Location:** `third_party/imgui/`
- **Usage:** Build once, link against `imgui.lib`
- **Build Once:** Yes

### 7. **glslang** (Shader Compiler)
- **Location:** `third_party/glslang/`
- **Usage:** Link against pre-built `glslang.lib`, `SPIRV.lib`
- **Build Once:** Yes

## Updating Third-Party Libraries

### When to Rebuild:

1. **Version Update:** Upgrading library to newer version
2. **Configuration Change:** Switching Debug ↔ Release
3. **Platform Change:** Moving Windows → Linux → Mac
4. **Compiler Change:** MSVC → Clang → GCC
5. **Bug Fix:** Known issue in current build

### How to Update:

```bash
# 1. Backup current binaries
cp -r third_party/library/lib third_party/library/lib.backup

# 2. Clean build directory
rm -rf third_party/library/build

# 3. Rebuild
cd third_party/library
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release

# 4. Verify new binaries work
cd ../../..
cmake --build build --target test

# 5. If successful, commit
git add third_party/library/include/  # Headers
git commit -m "chore: Update library to v1.2.3"

# Note: Pre-built binaries NOT committed (in .gitignore)
```

## .gitignore Configuration

The following patterns ensure build artifacts are ignored:

```gitignore
# Third-Party Libraries (keep source, ignore builds)
third_party/**/build/
third_party/**/bin/
third_party/**/lib/
third_party/**/*.lib
third_party/**/*.a
third_party/**/*.so
third_party/**/*.dylib
third_party/**/*.dll
third_party/**/*.obj
third_party/**/*.o

# External Pre-built Libraries
external/**/lib/*.lib
external/**/lib/*.a
external/**/bin/*.dll
external/**/bin/*.so
```

## CMake Best Practices

### ✅ **Recommended: Use Pre-Built Libraries**

```cmake
# Find pre-built library
find_library(GTEST_LIB
    NAMES gtest
    PATHS ${CMAKE_BINARY_DIR}/third_party/googletest/lib
    NO_DEFAULT_PATH
)

# Link against it
target_link_libraries(lore_tests PRIVATE ${GTEST_LIB})
```

### ❌ **Avoid: Rebuilding Every Time**

```cmake
# BAD: Rebuilds googletest on every CMake configure
add_subdirectory(third_party/googletest)
target_link_libraries(lore_tests PRIVATE gtest)
```

### ✅ **Alternative: FetchContent with Binary Cache**

```cmake
include(FetchContent)

# Only download if not present
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/v1.14.0.zip
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

# Use cached build if available
set(FETCHCONTENT_UPDATES_DISCONNECTED ON)
FetchContent_MakeAvailable(googletest)
```

## Troubleshooting

### Problem: "Cannot find library"

**Solution:** Build third-party libraries first
```bash
cmake -B build -S .
cmake --build build --config Release
```

### Problem: "Library version mismatch"

**Solution:** Rebuild third-party libraries
```bash
rm -rf build/third_party
cmake --build build --target rebuild_cache
```

### Problem: "Git keeps tracking *.lib files"

**Solution:** Remove from cache and update .gitignore
```bash
git rm --cached third_party/**/*.lib
git rm --cached external/**/*.lib
git commit -m "chore: Remove tracked library binaries"
```

## Summary

**Remember:**
1. Download/build third-party libraries **once**
2. Use pre-built binaries for all subsequent builds
3. Only rebuild when absolutely necessary
4. Never commit compiled binaries to git
5. Document all third-party dependencies

This approach keeps build times fast, repositories small, and builds consistent across the team.