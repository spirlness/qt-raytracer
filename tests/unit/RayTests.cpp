#include <gtest/gtest.h>
#include "RayTracer.h"

namespace {
constexpr double kEpsilon = 1e-9;
}

TEST(RayTests, AtReturnsPointAlongRay) {
    const Point3 origin(1.0, 2.0, 3.0);
    const Vec3 direction(0.0, 0.0, -2.0);
    const Ray ray(origin, direction);

    const Point3 point = ray.at(2.5);

    EXPECT_NEAR(point.x(), 1.0, kEpsilon);
    EXPECT_NEAR(point.y(), 2.0, kEpsilon);
    EXPECT_NEAR(point.z(), -2.0, kEpsilon);
}
