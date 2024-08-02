#include <Renderer.h>

int main() {
    Renderer renderer;
//    renderer.loadGltf("assets/models/milk_truck/CesiumMilkTruck.gltf");
//    renderer.loadGltf("assets/models/box/BoxTextured.gltf");
    renderer.loadGltf("assets/models/cesium_man/CesiumMan.gltf");
//    renderer.loadGltf("assets/models/brainstem/BrainStem.gltf");
//    renderer.loadGltf("assets/models/tests/SimpleSkin.gltf");
    renderer.loadGltf("assets/models/tests/RecursiveSkeletons.gltf"); // todo: handle this one
//    renderer.loadGltf("assets/models/box/BoxAnimated.gltf");
//    renderer.loadGltf("assets/models/tests/OrientationTest.gltf");
//    renderer.loadGltf("assets/models/Sponza/glTF/Sponza.gltf");
    renderer.run();
}