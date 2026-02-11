#include <gtest/gtest.h>
#include "RayTracer.h"
#include "tests/unit/TestHelpers.h"

namespace {
constexpr double kEpsilon = 1e-9;
}

TEST(HitableListTests, ReturnsClosestHit) {
    const auto material = std::make_shared<TestMaterial>();
    const auto near_sphere = std::make_shared<Sphere>(Point3(0.0, 0.0, -1.0), 0.5, material);
    const auto far_sphere = std::make_shared<Sphere>(Point3(0.0, 0.0, -2.0), 0.5, material);

    HitableList world;
    world.add(far_sphere);
    world.add(near_sphere);

    const Ray ray(Point3(0.0, 0.0, 0.0), Vec3(0.0, 0.0, -1.0));
    HitRecord rec;

    const bool hit = world.hit(ray, 0.001, infinity, rec);

    EXPECT_TRUE(hit);
    EXPECT_NEAR(rec.t, 0.5, kEpsilon);
}
