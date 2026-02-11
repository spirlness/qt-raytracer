#include <gtest/gtest.h>
#include "raytracer/RayTracer.h"
#include "unit/TestHelpers.h"

namespace {
constexpr double kEpsilon = 1e-9;
}

TEST(SphereTests, RayHitsSphereAtExpectedT) {
    const auto material = std::make_shared<TestMaterial>();
    const Sphere sphere(Point3(0.0, 0.0, -1.0), 0.5, material);

    const Ray ray(Point3(0.0, 0.0, 0.0), Vec3(0.0, 0.0, -1.0));
    HitRecord rec;

    const bool hit = sphere.hit(ray, 0.001, infinity, rec);

    EXPECT_TRUE(hit);
    EXPECT_NEAR(rec.t, 0.5, kEpsilon);
    EXPECT_TRUE(rec.front_face);
    EXPECT_NEAR(rec.normal.x(), 0.0, kEpsilon);
    EXPECT_NEAR(rec.normal.y(), 0.0, kEpsilon);
    EXPECT_NEAR(rec.normal.z(), 1.0, kEpsilon);
}

TEST(SphereTests, RayMissesSphere) {
    const auto material = std::make_shared<TestMaterial>();
    const Sphere sphere(Point3(0.0, 0.0, -1.0), 0.5, material);

    const Ray ray(Point3(0.0, 0.0, 0.0), Vec3(0.0, 1.0, 0.0));
    HitRecord rec;

    const bool hit = sphere.hit(ray, 0.001, infinity, rec);

    EXPECT_FALSE(hit);
}
