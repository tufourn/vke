#include <Renderer.h>

int main() {
    Renderer renderer;

    //todo: implement proper lighting system
    Light pointLight0 = {};
    pointLight0.position = {-10.f, 10.f, 10.f};
    renderer.addLight(pointLight0);

    uint32_t cesiumManId = renderer.loadGltf("assets/models/cesium_man/CesiumMan.gltf");
    renderer.addRenderObject({glm::mat4(1.f), cesiumManId});

    renderer.run();
}