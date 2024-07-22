#include <Renderer.h>

int main() {
    Renderer renderer;
    renderer.loadGltf("assets/models/milk_truck/CesiumMilkTruck.gltf");

    renderer.run();
}