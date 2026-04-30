# Technical Notes

This document summarizes the engineering decisions behind the renderer for reviewers who want more detail than the README.

## Rendering Pipeline

The renderer builds the Metal pipeline manually:

- initializes an AppKit application and `MTK::View`
- creates the Metal device and command queue
- compiles embedded Metal Shading Language source at startup
- creates render pipeline and depth-stencil state objects
- uploads vertex, index, material, camera, instance, and light buffers
- issues `drawIndexedPrimitives` each frame

The render loop keeps three frames in flight. Instance and camera data are stored in per-frame buffers, and a dispatch semaphore prevents the CPU from overwriting data still being consumed by the GPU.

## Model Loading

`SceneBuilder::importObjectFile` converts Wavefront OBJ data into GPU-ready mesh buffers.

Supported OBJ features:

- `v` positions
- `vn` normals
- `vt` texture coordinates
- `f` faces
- positive OBJ indices
- negative OBJ indices relative to the current end of each array
- triangles, quads, and n-gons

OBJ files frequently reuse the same position with different UV or normal indices. The importer handles that by deduplicating vertices with a composite key of `(positionIndex, texcoordIndex, normalIndex)`, then emitting a compact index buffer for Metal.

Malformed lines and invalid face vertices are skipped instead of crashing the renderer. If the final mesh is empty, the renderer falls back to procedural sphere generation.

## Material Loading

The `.mtl` parser maps the subset of material data used by the shader:

- `Kd` -> diffuse color
- `Ks` -> specular color
- `Ke` -> emissive color
- `Ns` -> Blinn-Phong shininess exponent
- `d` / `Tr` -> transparency-derived reflectivity hint

When no material file is present, the renderer supplies default material presets so lighting behavior is still visible.

## Coordinate Spaces And Normals

The vertex shader emits world-space positions and normals. The fragment shader also receives the camera position and point-light position in world space, so lighting vectors are computed in one consistent coordinate system.

Normal vectors use the inverse transpose of the model matrix's upper-left 3x3. This matters once non-uniform scale or combined rotations/scales are introduced; directly multiplying normals by the model matrix produces incorrect lighting.

## Shader Data Layout

The C++ structs under `shader_types` mirror the Metal shader structs:

- `VertexData`
- `InstanceData`
- `CameraData`
- `MaterialData`
- `LightData`

Keeping these definitions aligned is essential because Metal buffers are passed as raw memory. The explicit padding fields in material and light data avoid accidental layout mismatches.

## Procedural Mesh Fallback

The fallback sphere generator computes latitude/longitude vertices, normals, UVs, and indexed triangle lists. It gives the renderer a deterministic geometry path for debugging lighting, culling, and normal transforms even when an external model is unavailable.

