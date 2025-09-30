# NEONTECH - Perfect 1×1 Meter Blocks Quick Start

## 🎯 **Goal**: Create exactly 1×1×1 meter cubes for NEONTECH walls

---

## ⚡ **INSTANT SETUP (30 seconds)**

### **Step 1: Run Setup Script**
1. Open Blender
2. Go to **Scripting** workspace
3. Open: `tools/blender/setup_1x1_grid.py`
4. Click **Run Script** button

**Result**: Blender configured with 1-meter grid, reference cube created

---

## 📐 **MANUAL VERIFICATION**

### **Step 2: Check the Reference Cube**
- Look for object named: `WALL_BASIC_WHITE_1X1`
- **Properties Panel** → **Transform**
- **Dimensions should show**: X: 1.000m, Y: 1.000m, Z: 1.000m

### **Step 3: Verify Grid Alignment**
- **3D Viewport** → Press `N` to show properties
- **View** → **Display** → Enable **Grid**
- Grid lines should be exactly 1 meter apart
- Reference cube edges should align perfectly with grid lines

---

## 🔨 **CREATING YOUR FIRST 1×1 BLOCK**

### **Method 1: Use Reference Cube (RECOMMENDED)**
1. **Select** the reference cube (`WALL_BASIC_WHITE_1X1`)
2. **Duplicate**: `Shift + D`, then press `Enter`
3. **Move to position**: Use arrow keys (snaps to 1m grid)
4. **Rename**: Change name to your desired tile type
5. **Modify**: Edit geometry as needed
6. **Verify**: Dimensions stay 1.000×1.000×1.000

### **Method 2: Create from Scratch**
1. **Add Cube**: `Shift + A` → Mesh → Cube
2. **Check Size**: Properties panel should show 2.000×2.000×2.000
3. **Scale Down**: Press `S`, then type `0.5`, press `Enter`
4. **Apply Scale**: `Ctrl + A` → Scale  
5. **Verify**: Dimensions now 1.000×1.000×1.000

---

## ✅ **VALIDATION CHECKLIST**

Before creating more models, verify:

### **Dimensions**
- [ ] X: 1.000m (exactly)
- [ ] Y: 1.000m (exactly) 
- [ ] Z: 1.000m (exactly)

### **Position**
- [ ] Location on grid intersection (X.000, Y.000, Z.000)
- [ ] No decimal places in location coordinates
- [ ] Snaps correctly to grid when moved

### **Scale**
- [ ] Scale: 1.000, 1.000, 1.000 (scale applied)
- [ ] No modifiers affecting size
- [ ] Transform properly applied

### **Visual Check**
- [ ] Cube edges align with grid lines perfectly
- [ ] No gaps or overlaps with adjacent grid positions
- [ ] Looks like exactly one grid square in each direction

---

## 🚀 **QUICK VALIDATION SCRIPT**

Run this in Blender console to check your work:
```python
# Quick validation
obj = bpy.context.active_object
dims = obj.dimensions
print(f"Dimensions: {dims.x:.3f}×{dims.y:.3f}×{dims.z:.3f} meters")

# Perfect 1×1×1 check
is_perfect = all(abs(d - 1.0) < 0.001 for d in dims)
print("✅ PERFECT 1×1×1 METER BLOCK!" if is_perfect else "❌ Size incorrect")
```

---

## 🛠️ **COMMON FIXES**

### **Problem**: Cube is 2×2×2 meters
**Fix**: Scale by 0.5, then apply scale (`Ctrl + A` → Scale)

### **Problem**: Cube is 0.8×0.8×1.2 meters  
**Fix**: Manually set dimensions in Properties → Transform → Dimensions

### **Problem**: Not aligned to grid
**Fix**: Enable snap to grid (`Shift + Tab`), move to grid intersection

### **Problem**: Scale shows 0.500, 0.500, 0.500
**Fix**: Apply scale transform (`Ctrl + A` → Scale)

---

## 📊 **VERIFICATION MEASUREMENTS**

### **In Blender Properties Panel**
```
Transform:
  Location: X: 0.000m  Y: 0.000m  Z: 0.500m  
  Rotation: X: 0.000°  Y: 0.000°  Z: 0.000°
  Scale:    X: 1.000   Y: 1.000   Z: 1.000
  
Dimensions:
  X: 1.000m  Y: 1.000m  Z: 1.000m
```

### **Visual Verification**
- Cube occupies exactly one grid square
- Edges align with major grid lines
- No overlap with neighboring grid positions
- Looks proportional and correct

---

## 🎯 **SUCCESS CRITERIA**

✅ **You have a perfect 1×1×1m block when**:
1. Dimensions show exactly 1.000m × 1.000m × 1.000m
2. Scale shows 1.000, 1.000, 1.000 (applied)
3. Cube edges align perfectly with grid lines
4. Object snaps cleanly to grid intersections
5. Looks like it would fit exactly in one NEONTECH game tile

**Once you have this perfect reference, duplicate it for all future wall blocks!**

---

## 🔄 **WORKFLOW**
1. **Run setup script** → Get perfect reference cube
2. **Duplicate reference** → `Shift + D` for new blocks  
3. **Position on grid** → Move to desired grid intersection
4. **Modify geometry** → Edit shape while keeping 1×1×1 bounds
5. **Rename appropriately** → Follow NEONTECH naming convention
6. **Export** → Use batch processor for optimized export

This ensures every block you create will fit perfectly in NEONTECH's 1-meter tile grid system!