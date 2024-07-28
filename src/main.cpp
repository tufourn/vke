#include <Renderer.h>

int main() {
    Renderer renderer;
//     renderer.loadGltf("assets/models/milk_truck/CesiumMilkTruck.gltf");
//     renderer.loadGltf("assets/models/box/BoxTextured.gltf");
//     renderer.loadGltf("assets/models/box/BoxInterleaved.gltf"); // interleaved stopped working because its material has no BaseColorTexture
    renderer.loadGltf("assets/models/Sponza/glTF/Sponza.gltf");
    renderer.run();
}