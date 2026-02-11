#include <gtest/gtest.h>

#include "raytracer/RayTracer.h"

namespace {
constexpr double kEpsilon = 1e-9;
}

TEST(MathUtilsTests, DegreesToRadiansHandlesKnownAngles) {
    EXPECT_NEAR(degrees_to_radians(0.0), 0.0, kEpsilon);
    EXPECT_NEAR(degrees_to_radians(90.0), pi / 2.0, kEpsilon);
    EXPECT_NEAR(degrees_to_radians(180.0), pi, kEpsilon);
}

TEST(MathUtilsTests, ClampLimitsToRange) {
    EXPECT_NEAR(clamp(-2.0, 0.0, 1.0), 0.0, kEpsilon);
    EXPECT_NEAR(clamp(0.25, 0.0, 1.0), 0.25, kEpsilon);
    EXPECT_NEAR(clamp(9.0, 0.0, 1.0), 1.0, kEpsilon);
}

TEST(MathUtilsTests, RandomInUnitSphereStaysInsideUnitSphere) {
    for (int i = 0; i < 256; ++i) {
        const Vec3 p = random_in_unit_sphere();
        EXPECT_LT(p.length_squared(), 1.0);
    }
}

TEST(MathUtilsTests, RandomInUnitDiskStaysInsideDiskPlane) {
    for (int i = 0; i < 256; ++i) {
        const Vec3 p = random_in_unit_disk();
        EXPECT_LT(p.length_squared(), 1.0);
        EXPECT_NEAR(p.z(), 0.0, kEpsilon);
    }
}
