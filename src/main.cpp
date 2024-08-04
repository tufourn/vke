#include <Renderer.h>

int main() {
    Renderer renderer;

    Light pointLight0 = {};
    pointLight0.direction = {10.f, 10.f, 10.f};

    Light pointLight1 = {};
    pointLight1.direction = {-5.f, 10.f, 10.f};

    renderer.addLight(pointLight0);
    renderer.addLight(pointLight1);

//    renderer.loadGltf("assets/models/milk_truck/CesiumMilkTruck.gltf");
//    renderer.loadGltf("assets/models/box/BoxTextured.gltf");
    renderer.loadGltf("assets/models/cesium_man/CesiumMan.gltf");
//    renderer.loadGltf("assets/models/brainstem/BrainStem.gltf");
//    renderer.loadGltf("assets/models/box/AnimatedMorphCube.gltf");
//    renderer.loadGltf("assets/models/tests/SimpleSkin.gltf");
//    renderer.loadGltf("assets/models/tests/RecursiveSkeletons.gltf");
//    renderer.loadGltf("assets/models/box/BoxAnimated.gltf");
//    renderer.loadGltf("assets/models/tests/OrientationTest.gltf");
//    renderer.loadGltf("assets/models/Sponza/glTF/Sponza.gltf");
//    renderer.loadGltf("assets/models/helmet/DamagedHelmet.gltf");
    renderer.run();
}