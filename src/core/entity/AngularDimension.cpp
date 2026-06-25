#include "entity/AngularDimension.hpp"

#include <cmath>

namespace cadino::core {

double AngularDimension::angle_rad() const noexcept {
    const double a1 = std::atan2(p1.y() - vertex.y(), p1.x() - vertex.x());
    const double a2 = std::atan2(p2.y() - vertex.y(), p2.x() - vertex.x());
    double d = a2 - a1;
    while (d < 0.0) d += 2.0 * 3.14159265358979323846;
    while (d > 2.0 * 3.14159265358979323846) d -= 2.0 * 3.14159265358979323846;
    return d;
}

}  // namespace cadino::core
