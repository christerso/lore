# Task Executor 1: Rendering Pipeline Integration

## Assignment
Complete the integration of atmospheric scattering, HDR buffer management, and unified rendering loop in the DeferredRenderer system.

## Objective
Wire atmospheric uniforms into the lighting shader, add HDR buffer creation/management, and create a unified rendering example showing all systems working together.

## Task Breakdown

### Subtask 1.1: Add Atmospheric Uniforms to Lighting Shader
**File**: `shaders/deferred_lighting.frag`
**Requirements**:
- Add `AtmosphericData` uniform buffer (binding 2)
- Include atmospheric scattering calculation function
- Apply atmospheric fog based on fragment distance
- Modulate lighting output with atmospheric color
- Preserve HDR output range

**Atmospheric Data Structure** (must match C++ side):
```glsl
layout(set = 0, binding = 2) uniform AtmosphericData {
    vec3 rayleigh_scattering;       // Rayleigh scattering coefficients (R,G,B)
    float rayleigh_scale_height;    // Scale height in meters
    vec3 mie_scattering;            // Mie scattering coefficients
    float mie_scale_height;         // Scale height in meters
    vec3 sun_direction;             // Primary celestial body direction
    float sun_intensity;            // Sun brightness multiplier
    vec3 sun_color;                 // Sun color tint
    float planet_radius;            // Planet radius in meters
    vec3 atmosphere_extinction;     // Combined extinction
    float atmosphere_radius;        // Atmosphere outer radius
} u_atmospheric;
```

**Integration Points**:
1. Calculate view direction from camera to fragment
2. Compute fragment distance from camera
3. Apply atmospheric scattering based on distance
4. Blend atmospheric color with lit fragment color
5. Preserve specular highlights (don't fog them out completely)

### Subtask 1.2: Add HDR Buffer Management to DeferredRenderer
**Files**:
- `include/lore/graphics/deferred_renderer.hpp`
- `src/graphics/deferred_renderer.cpp`

**Requirements**:
- Add `create_hdr_buffer(VkExtent2D extent)` method
- Use `VK_FORMAT_R16G16B16A16_SFLOAT` for HDR precision
- Create framebuffer for lighting pass output
- Add `resize_hdr_buffer()` for window resize handling
- Expose `get_hdr_image()` and `get_hdr_image_view()` accessors

**HDR Buffer Specification**:
```cpp
struct HDRBuffer {
    VkImage image = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;
    VmaAllocation allocation = VK_NULL_HANDLE;
    VkFramebuffer framebuffer = VK_NULL_HANDLE;
    VkExtent2D extent = {0, 0};
};
```

**Methods to Add**:
```cpp
void create_hdr_buffer(VkExtent2D extent);
void destroy_hdr_buffer();
void resize_hdr_buffer(VkExtent2D new_extent);
VkImage get_hdr_image() const { return hdr_buffer_.image; }
VkImageView get_hdr_image_view() const { return hdr_buffer_.view; }
```

### Subtask 1.3: Add Atmospheric Descriptor Set
**File**: `src/graphics/deferred_renderer.cpp`

**Requirements**:
- Add atmospheric descriptor set layout (binding 2, uniform buffer)
- Allocate atmospheric descriptor set in `create_descriptor_sets()`
- Add `update_atmospheric_descriptor_set(VkBuffer buffer)` method
- Bind atmospheric descriptor set in lighting pass

**Descriptor Layout**:
```cpp
// In create_descriptor_sets()
VkDescriptorSetLayoutBinding atmospheric_binding{};
atmospheric_binding.binding = 2;
atmospheric_binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
atmospheric_binding.descriptorCount = 1;
atmospheric_binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
```

### Subtask 1.4: Update Lighting Pipeline for Atmospheric
**File**: `src/graphics/deferred_renderer.cpp` - `create_lighting_pipeline()`

**Requirements**:
- Add atmospheric descriptor set layout to pipeline layout
- Update descriptor set count to 3 (G-Buffer, Lights, Atmospheric)
- Ensure binding order matches shader expectations

### Subtask 1.5: Update Complete Rendering Demo
**File**: `examples/complete_rendering_demo.cpp`

**Requirements**:
- Show atmospheric uniform buffer creation
- Demonstrate updating atmospheric data each frame
- Show HDR buffer usage in rendering loop
- Add atmospheric parameter tweaking (runtime controls)
- Document integration pattern clearly

**Example Integration**:
```cpp
// Create atmospheric uniform buffer
VkBufferCreateInfo atmos_buffer_info{};
// ... buffer creation ...

// Update atmospheric data
auto& atmos_comp = world_.get_component<AtmosphericComponent>(atmos_entity);
AtmosphericData atmos_data = atmos_comp.to_gpu_data();
memcpy(atmos_mapped_ptr, &atmos_data, sizeof(AtmosphericData));

// Update descriptor set
deferred_renderer_->update_atmospheric_descriptor_set(atmos_buffer);

// Render with atmospheric
deferred_renderer_->begin_lighting_pass(cmd, hdr_buffer.image, hdr_buffer.view);
deferred_renderer_->end_lighting_pass(cmd);
```

## Success Criteria
1. ✅ Atmospheric uniforms successfully integrated in lighting shader
2. ✅ HDR buffer created and managed by DeferredRenderer
3. ✅ Atmospheric descriptor set created and bound correctly
4. ✅ Complete rendering demo compiles without errors
5. ✅ Demo shows atmospheric perspective on distant objects
6. ✅ All systems work together (deferred + shadows + atmospheric + post-process)

## Dependencies
- AtmosphericComponent exists in `include/lore/ecs/components/atmospheric_component.hpp`
- DeferredRenderer is production-ready
- Lighting shader already outputs HDR values

## Context
- Project root: `G:\repos\lore`
- Branch: `feature/complete-integration`
- All rendering systems are individually complete
- This task wires them together into unified pipeline

## Reporting
When complete, report back with:
1. List of modified files
2. Compilation status
3. Any issues encountered
4. Suggestions for testing

## Executor Instructions
- Read all mentioned files first
- Make changes incrementally (shader → C++ → example)
- Test compilation after each major change
- Follow existing code style and patterns
- Add comprehensive comments for integration points
- DO NOT simplify or take shortcuts
- Implement FULL functionality as specified