# Metal Render Pipeline
AJ Matheson-Lieber

![Final Render](./Documentation/render-pipeline.png)


## Dependencies

These samples include the **metal-cpp** and **metal-cpp-extensions** libraries.

Use either the included Xcode project or the UNIX make utility to build the project.

This project requires C++17 support (available since Xcode 9.3).

## Building with Make

To build the samples using a Makefile, open the terminal and run the `make` command. The build process will put the executables into the `build/` folder.

By default, the Makefile compiles the source with the `-O2` optimization level. Pass the following options to make change the build configuration:

* `DEBUG=1` : disable optimizations and include symbols (`-g`).
* `ASAN=1` : build with address sanitizer support (`-fsanitize=address`).

# Introduction
For this project, I created a simple render pipeline in Metal using C++. I decided to pursue this project, because I missed coding in c++ and I wanted the chance to explore graphics APIs. My codebase is based on the Learn Metal C++ Template @ https://developer.apple.com/metal/sample-code/. On top of this template, I implemented point-based Blinn Phong lighting, added sphere generation, simple camera movement, and 3 custom material presets. 

Discussion
This project gave me a much deeper understanding of how a rendering pipeline functions beneath the abstractions of a high-level engine. I successfully implemented point-based Blinn–Phong lighting, generated custom sphere geometry, created multiple material presets, and built a functioning camera system within a fully manual C++/Metal pipeline. These features allowed me to produce a small but complete real-time rendering environment that I fully controlled, from data structures on the CPU to shading calculations on the GPU. If I had more time, I would have expanded the system in two main directions: visual quality and engine architecture. On the visual side, I would have implemented a skybox, reflections, and more advanced material shaders to push the lighting fidelity further. Architecturally, I would have liked to modularize the pipeline into clearer components like meshes, materials, and scene builder elements in order to get closer to the structure of a real engine. Additional improvements such as smoother camera input, more geometry types, and UI-based material switching would also have enhanced usability and expressiveness. Overall, the project demonstrated that I could meaningfully extend a low-level rendering template, and it highlighted exactly which areas I would pursue to transform it from a technical demonstration into a more polished, engine-like system.
Sources

https://developer.apple.com/metal/sample-code/
https://developer.apple.com/documentation/Metal/
