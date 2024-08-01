#include <Renderer.h>

int main() {
    Renderer renderer;
//    renderer.loadGltf("assets/models/milk_truck/CesiumMilkTruck.gltf");
//    renderer.loadGltf("assets/models/box/BoxTextured.gltf");
//    renderer.loadGltf("assets/models/cesium_man/CesiumMan.gltf");
//    renderer.loadGltf("assets/models/box/BoxAnimated.gltf");
    renderer.loadGltf("assets/models/tests/OrientationTest.gltf");
//    renderer.loadGltf("assets/models/Sponza/glTF/Sponza.gltf");
    renderer.run();
}