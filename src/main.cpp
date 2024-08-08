#include <Renderer.h>

// main looks like a mess right now because it mostly contains code for testing the renderer

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

    MeshBuffers cubeMesh = createCubeMesh(1.0, 1.0, 1.0);
    uint32_t cubeId = renderer.loadGeneratedMesh(&cubeMesh);
//    MeshBuffers sphereMesh = createSphereMesh(1.0, 20, 20);
//    renderer.loadGeneratedMesh(&sphereMesh);

//    uint32_t id = renderer.loadGltf("assets/models/milk_truck/CesiumMilkTruck.gltf");
//    uint32_t id = renderer.loadGltf("assets/models/box/BoxTextured.gltf");
//    uint32_t id = renderer.loadGltf("assets/models/tests/InterpolationTest.glb");
//    uint32_t id = renderer.loadGltf("assets/models/box/AnimatedMorphCube.gltf");
//    uint32_t id = renderer.loadGltf("assets/models/tests/SimpleSkin.gltf");
//    uint32_t id = renderer.loadGltf("assets/models/tests/RecursiveSkeletons.gltf");
    uint32_t id = renderer.loadGltf("assets/models/box/BoxAnimated.gltf");
//    uint32_t id = renderer.loadGltf("assets/models/tests/OrientationTest.gltf");
//    uint32_t id = renderer.loadGltf("assets/models/Sponza/glTF/Sponza.gltf");

//    renderer.loadGltf("assets/models/triangle/Triangle.gltf");
    uint32_t cesiumManId = renderer.loadGltf("assets/models/cesium_man/CesiumMan.gltf");
    uint32_t helmetId = renderer.loadGltf("assets/models/helmet/DamagedHelmet.gltf");

    renderer.addRenderObject({glm::mat4(1.f), cubeId});

    for (int x = 0; x < 10; ++x) {
        for (int z = 0; z < 10; ++z) {
            glm::mat4 transformCesium = glm::translate(glm::mat4(1.0f), glm::vec3(x * 2.0f, 0.0f, z * 2.0f));
            renderer.addRenderObject({transformCesium, cesiumManId});

            glm::mat4 transformHelmet = glm::translate(glm::mat4(1.0f), glm::vec3(x * 2.0f, 4.0f, z * 2.0f));
            renderer.addRenderObject({transformHelmet, helmetId});

            glm::mat4 transformCube = glm::translate(glm::mat4(1.0f), glm::vec3(x * 2.0f, -2.0f, z * 2.0f));
            renderer.addRenderObject({transformCube, cubeId});
        }
    }

//    renderer.loadGltf("assets/models/tests/MetalRoughSpheres.gltf");
//    renderer.loadGltf("assets/models/tests/MetalRoughSpheresNoTextures.gltf"); // wrong transformation?
//    renderer.loadGltf("assets/models/tests/NormalTangentMirrorTest.gltf");
//    renderer.loadGltf("assets/models/tests/NormalTangentTest.gltf");
//    renderer.loadGltf("assets/models/tests/TextureSettingsTest.gltf");
    renderer.run();
}