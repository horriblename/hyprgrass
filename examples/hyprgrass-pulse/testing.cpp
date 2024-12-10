#include "pulse.hpp"

int main(int argc, char* argv[]) {
    auto b = AudioBackend::getInstance();
    while (true) {
        int a;
        scanf("%d", &a);
        if (a == 0) {
            return 0;
        } else if (a == 1) {
            b->changeVolume(ChangeType::Increase, 5);
        } else {
            b->changeVolume(ChangeType::Decrease, 5);
        }
    }
}
