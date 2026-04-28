#ifndef QMD_ROTATION_HH
#define QMD_ROTATION_HH

#include <cmath>

namespace qmd {

inline double phi_np(double sum_px, double sum_py) {
    return std::atan2(sum_py, sum_px);
}

inline void rotate_xy(double& x, double& y, double angle) {
    const double c = std::cos(angle);
    const double s = std::sin(angle);
    const double xr = c * x - s * y;
    const double yr = s * x + c * y;
    x = xr;
    y = yr;
}

inline double wrap_two_pi(double angle) {
    constexpr double two_pi = 2.0 * M_PI;
    const double r = std::fmod(angle, two_pi);
    return (r < 0.0) ? r + two_pi : r;
}

}  // namespace qmd

#endif
