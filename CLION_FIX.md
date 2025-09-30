# CLion Configuration Fix

## The Problem
CLion was trying to use Ninja generator with the MSVC compiler, which causes issues with standard library headers (`<string>`, etc.).

## The Solution
I've created a `CMakePresets.json` file that **forces** CLion to use Visual Studio 2022 generator.

## What You Need to Do in CLion

### Step 1: Close and Reopen CLion
Close CLion completely and reopen the project. This ensures it picks up the new CMakePresets.json.

### Step 2: Select the Correct Preset
1. In CLion, go to: **File → Settings → Build, Execution, Deployment → CMake**
2. You should see two presets:
   - `debug` (using Visual Studio 17 2022)
   - `release` (using Visual Studio 17 2022)
3. **Enable the `debug` preset** and **disable any Ninja-based profiles**
4. Click **Apply** and **OK**

### Step 3: Reload CMake
1. Go to: **File → Reload CMake Project**
2. Or click the reload button in the CMake tool window

### Step 4: Verify
In the CMake output, you should see:
```
-- The C compiler identification is MSVC 19.44.35216.0
-- The CXX compiler identification is MSVC 19.44.35216.0
```

And it should **NOT** say:
```
-G Ninja
```

## Alternative: Disable Ninja in Toolchain Settings

If CLion still tries to use Ninja:

1. **File → Settings → Build, Execution, Deployment → Toolchains**
2. Select your **Visual Studio** toolchain (should be default)
3. Make sure it's at the **top** of the list
4. **File → Settings → Build, Execution, Deployment → CMake**
5. For each profile, ensure **Generator** is set to **"Let CMake decide"** or **"Visual Studio 17 2022"**
6. Do NOT set it to "Ninja"

## Files Created

- `CMakePresets.json` - Forces Visual Studio 2022 generator (version controlled)
- `.idea/cmake.xml` - CLion-specific CMake settings
- `.idea/CLION_SETUP.md` - Documentation (you can delete this)
- `CLION_FIX.md` - This file (you can delete this after fixing)

## Verify Build Works

After reloading:
```bash
cd cmake-build-debug
cmake --build . --config Debug --target lore
```

Should output:
```
lore.vcxproj -> G:\repos\lore\cmake-build-debug\Debug\lore.exe
```

## Still Having Issues?

If it STILL tries to use Ninja, manually delete:
```bash
rm -rf cmake-build-debug
```

Then in CLion: **File → Reload CMake Project**

CLion will recreate cmake-build-debug using the Visual Studio generator from CMakePresets.json.