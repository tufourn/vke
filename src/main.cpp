#include <Renderer.h>

int main() {
    Renderer renderer;

    //todo: implement proper lighting system
    Light pointLight0 = {};
    pointLight0.position = {-10.f, 10.f, 10.f};
    renderer.addLight(pointLight0);

//    Light pointLight1 = {};
//    pointLight1.position = {-5.f, 10.f, -10.f};
//
//    renderer.addLight(pointLight1);

//    MeshBuffers cubeMesh = createCubeMesh(2.0, 2.0, 2.0);
//    renderer.loadGeneratedMesh(&cubeMesh);

    MeshBuffers sphereMesh = createSphereMesh(1.0, 20, 20);
    renderer.loadGeneratedMesh(&sphereMesh);

//    renderer.loadGltf("assets/models/milk_truck/CesiumMilkTruck.gltf");
//    renderer.loadGltf("assets/models/box/BoxTextured.gltf");
//    renderer.loadGltf("assets/models/cesium_man/CesiumMan.gltf");
//    renderer.loadGltf("assets/models/brainstem/BrainStem.gltf");
//    renderer.loadGltf("assets/models/box/AnimatedMorphCube.gltf");
//    renderer.loadGltf("assets/models/tests/SimpleSkin.gltf");
//    renderer.loadGltf("assets/models/tests/RecursiveSkeletons.gltf");
//    renderer.loadGltf("assets/models/box/BoxAnimated.gltf");
//    renderer.loadGltf("assets/models/tests/OrientationTest.gltf");
//    renderer.loadGltf("assets/models/Sponza/glTF/Sponza.gltf");
//    renderer.loadGltf("assets/models/triangle/Triangle.gltf");
//    renderer.loadGltf("assets/models/helmet/DamagedHelmet.gltf");
//    renderer.loadGltf("assets/models/tests/MetalRoughSpheres.gltf");
//    renderer.loadGltf("assets/models/tests/MetalRoughSpheresNoTextures.gltf"); // wrong transformation?
//    renderer.loadGltf("assets/models/tests/NormalTangentMirrorTest.gltf");
//    renderer.loadGltf("assets/models/tests/NormalTangentTest.gltf");
//    renderer.loadGltf("assets/models/tests/TextureSettingsTest.gltf");
    renderer.run();
}