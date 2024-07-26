#include <Renderer.h>

int main() {
    Renderer renderer;
    // renderer.loadGltf("assets/models/milk_truck/CesiumMilkTruck.gltf");
    // renderer.loadGltf("assets/models/triangle/TriangleWithoutIndices.gltf");
    renderer.loadGltf("assets/models/Sponza/glTF/Sponza.gltf");

    renderer.run();
}