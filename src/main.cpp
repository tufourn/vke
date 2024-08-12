#include <Renderer.h>

int main() {
    Renderer renderer;

    //todo: implement proper lighting system

    for (int i = 0; i < 50; ++i) {
        Light pointLight = {};
        pointLight.position = {
                static_cast<float>(rand() % 100 - 50),  // X position between -50 and 50
                static_cast<float>(rand() % 100),  // Y position between 0 and 100
                static_cast<float>(rand() % 100 - 50)   // Z position between -50 and 50
        };
        renderer.addLight(pointLight);
    }

//    uint32_t id = renderer.loadGltf("assets/models/tests/MetalRoughSpheres.gltf");
    uint32_t id = renderer.loadGltf("assets/models/helmet/DamagedHelmet.gltf");
    renderer.addRenderObject({glm::mat4(1.f), id});

    renderer.run();
}