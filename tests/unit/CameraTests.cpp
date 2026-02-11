#include <gtest/gtest.h>

#include "raytracer/RayTracer.h"

namespace {
constexpr double kEpsilon = 1e-9;
}

TEST(CameraTests, CenterRayPointsAtLookAtWithZeroAperture) {
    const Point3 lookfrom(0.0, 0.0, 0.0);
    const Point3 lookat(0.0, 0.0, -1.0);
    const Vec3 vup(0.0, 1.0, 0.0);

    Camera camera(lookfrom, lookat, vup, 90.0, 2.0, 0.0, 1.0);

    const Ray ray = camera.get_ray(0.5, 0.5);
    EXPECT_NEAR(ray.origin().x(), 0.0, kEpsilon);
    EXPECT_NEAR(ray.origin().y(), 0.0, kEpsilon);
    EXPECT_NEAR(ray.origin().z(), 0.0, kEpsilon);
    EXPECT_NEAR(ray.direction().x(), 0.0, kEpsilon);
    EXPECT_NEAR(ray.direction().y(), 0.0, kEpsilon);
    EXPECT_NEAR(ray.direction().z(), -1.0, kEpsilon);
}

TEST(CameraTests, LensOffsetStaysWithinApertureRadius) {
    const Point3 lookfrom(0.0, 0.0, 0.0);
    const Point3 lookat(0.0, 0.0, -1.0);
    const Vec3 vup(0.0, 1.0, 0.0);
    const double aperture = 2.0;

    Camera camera(lookfrom, lookat, vup, 90.0, 2.0, aperture, 1.0);

    for (int i = 0; i < 128; ++i) {
        const Ray ray = camera.get_ray(0.5, 0.5);
        const Vec3 offset = ray.origin() - lookfrom;
        EXPECT_LE(offset.length(), aperture * 0.5 + kEpsilon);
        EXPECT_NEAR(offset.z(), 0.0, kEpsilon);
    }
}
