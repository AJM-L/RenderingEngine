# Metal C++ Rendering Engine

AJ Matheson-Lieber

[![Final render](Documentation/custom-model.png)](https://youtu.be/RUhq3tnou9s)

A real-time 3D renderer built directly on Apple's Metal API from C++17. This project started from Apple's `metal-cpp` sample scaffolding and extends it into a custom rendering pipeline with model loading, material parsing, procedural geometry fallback, per-frame GPU resource management, and dynamic Blinn-Phong lighting.

The goal of this repository is to demonstrate systems-level software engineering in a graphics context: explicit memory ownership, GPU/CPU synchronization, file-format parsing, shader development, and debugging across coordinate spaces without relying on a game engine.

## Technical Highlights

- **Direct Metal API usage from C++:** Creates the AppKit window, `MTK::View`, Metal device, command queue, render pipeline state, depth-stencil state, buffers, and draw calls manually through `metal-cpp`.
- **Triple-buffered frame resources:** Maintains per-frame instance and camera buffers behind a dispatch semaphore to avoid CPU/GPU write hazards while keeping frames in flight.
- **Custom OBJ importer:** Parses Wavefront `.obj` vertex positions, normals, UVs, positive and negative indices, triangles, quads, and n-gons. Faces are triangulated with a fan strategy and vertices are deduplicated by `(position, texcoord, normal)` tuples before upload.
- **MTL material parser:** Reads diffuse, specular, emissive, shininess, and transparency-derived reflectivity data from `.mtl` files, with robust defaults when material data is absent.
- **Runtime Metal shader compilation:** Embeds Metal Shading Language source, compiles vertex/fragment functions at startup, and binds shared C++/shader layout structs for vertex, instance, camera, material, and light data.
- **Point-light Blinn-Phong shading:** Computes world-space normals, view vectors, point-light vectors, diffuse response, Blinn-Phong specular highlights, ambient contribution, and emissive material output in the fragment shader.
- **Procedural geometry fallback:** Generates a UV sphere with indexed triangles, smooth normals, and UV coordinates when model loading fails, keeping the renderer demonstrable without external assets.
- **Camera and light animation:** Uses custom matrix math for perspective, look-at, scale, translation, rotations, and inverse-transpose normal transforms.

## Demo

The project includes screenshots under `Documentation/` and a short render capture linked from the image above.

![Render pipeline diagram](Documentation/render-pipeline.png)

By default, the renderer attempts to load the bundled sample model at `build/objects/Monkey.obj`. If that load fails or the mesh is empty, it falls back to a generated sphere.

## Build And Run

Requirements:

- macOS with Metal support
- Xcode command line tools
- Clang with C++17 support

Build:

```sh
make
```

Run from the repository root so relative asset paths resolve correctly:

```sh
make run
```

Optional build modes:

```sh
make DEBUG=1
make ASAN=1
```

Clean generated objects and executable:

```sh
make clean
```

## Architecture

```text
AppKit application
  -> MTK::View draw loop
     -> Renderer
        -> runtime shader compilation
        -> depth/pipeline state creation
        -> mesh/material/light/camera buffer upload
        -> per-frame animation and drawIndexedPrimitives

SceneBuilder
  -> OBJ parsing and vertex deduplication
  -> procedural sphere fallback

Shader pipeline
  -> vertex shader: object -> world -> view -> clip transforms
  -> fragment shader: material lookup + point-light Blinn-Phong lighting
```

More implementation detail is available in [Documentation/TECHNICAL_NOTES.md](Documentation/TECHNICAL_NOTES.md).

## What This Shows

This repository is intentionally low-level. Instead of calling into a rendering engine, it implements the pieces that engines usually hide:

- translating file-format data into GPU-ready vertex/index buffers
- managing object lifetimes across C++ wrappers for Objective-C Metal objects
- synchronizing CPU writes with in-flight GPU command buffers
- keeping transform math consistent between CPU code and shader code
- debugging visual output caused by normals, winding, coordinate spaces, and material parameters

## Current Limitations

- Texture maps are parsed as UV coordinates but not sampled in the shader yet.
- The material system is Blinn-Phong rather than physically based rendering.
- The renderer currently displays one loaded mesh instance rather than a full scene graph.
- OBJ loading is synchronous at startup and intended for small showcase assets.

## Credits

- Apple `metal-cpp` headers and sample scaffolding: https://developer.apple.com/metal/cpp/
- Metal documentation and sample code: https://developer.apple.com/metal/

