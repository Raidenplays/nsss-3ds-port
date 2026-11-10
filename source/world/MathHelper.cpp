#include "MathHelper.h"

namespace MathHelper {
const float* sinTable() {
    static float* t = nullptr;
    if (!t) {
        t = new float[65536];
        for (int i = 0; i < 65536; ++i) t[i] = float(std::sin(double(i) * 3.141592653589793 * 2.0 / 65536.0));
    }
    return t;
}
} // namespace MathHelper
