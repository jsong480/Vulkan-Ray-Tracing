#include "app.h"

int main() {
    try {
        App app;
        app.run();
    } catch (const std::exception& e) {
        std::cerr << "[Fatal] " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
