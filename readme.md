# Vulkan renderer

## About
A Vulkan renderer. It's primarily a playground for me to learn and try to implement modern graphics programming techniques. 

### Screenshots
![helmet](screenshots/helmet.gif)
![cesium_man](screenshots/cesium_man.gif)

## Features
- [x] Bindless resources
- [x] glTF scene loading
- [x] Physically-based rendering
- [x] Animations with vertex skinning
- [x] Normal mapping, with [MikkTSpace](https://github.com/mmikk/MikkTSpace) tangent calculation
- [x] Environment cubemap (curnently only supports equirectangular images)

## Todos
- [ ] Image based lighting
- [ ] Shadow mapping
- [ ] Compute based culling

## Build instructions
### Cloning
The repository contains submodules for external dependencies. Clone the project recursively:
```
git clone --recursive https://github.com/tufourn/vke.git
```

Or if you forgot the `--recursive`:
```
git submodule update --init
```

### Building
The project is built using CMake. Use the provided CMakeLists.txt to generate a build configuration.