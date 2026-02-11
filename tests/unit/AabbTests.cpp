#include <gtest/gtest.h>

#include <memory>

#include "raytracer/RayTracer.h"

namespace {
constexpr double kEpsilon = 1e-9;
}

TEST(AabbTests, HitReturnsTrueWhenRayIntersectsBox) {
    const AABB box(Point3(-1.0, -1.0, -3.0), Point3(1.0, 1.0, -1.0));
    const Ray ray(Point3(0.0, 0.0, 0.0), Vec3(0.0, 0.0, -1.0));

    EXPECT_TRUE(box.hit(ray, 0.001, infinity));
}

TEST(AabbTests, HitReturnsFalseWhenRayMissesBox) {
    const AABB box(Point3(-1.0, -1.0, -3.0), Point3(1.0, 1.0, -1.0));
    const Ray ray(Point3(0.0, 2.0, 0.0), Vec3(0.0, 0.0, -1.0));

    EXPECT_FALSE(box.hit(ray, 0.001, infinity));
}

TEST(AabbTests, SurroundingBoxContainsBothInputs) {
    const AABB box0(Point3(-1.0, -2.0, -3.0), Point3(0.5, 0.0, -1.0));
    const AABB box1(Point3(-0.25, -1.0, -6.0), Point3(2.0, 3.0, 0.5));
    const AABB merged = surrounding_box(box0, box1);

    EXPECT_NEAR(merged.min().x(), -1.0, kEpsilon);
    EXPECT_NEAR(merged.min().y(), -2.0, kEpsilon);
    EXPECT_NEAR(merged.min().z(), -6.0, kEpsilon);
    EXPECT_NEAR(merged.max().x(), 2.0, kEpsilon);
    EXPECT_NEAR(merged.max().y(), 3.0, kEpsilon);
    EXPECT_NEAR(merged.max().z(), 0.5, kEpsilon);
}

TEST(AabbTests, SphereBoundingBoxMatchesRadiusAroundCenter) {
    const auto material = std::make_shared<Lambertian>(Color(0.2, 0.3, 0.4));
    const Sphere sphere(Point3(1.0, -2.0, -3.5), 0.75, material);
    AABB box;

    EXPECT_TRUE(sphere.bounding_box(box));
    EXPECT_NEAR(box.min().x(), 0.25, kEpsilon);
    EXPECT_NEAR(box.min().y(), -2.75, kEpsilon);
    EXPECT_NEAR(box.min().z(), -4.25, kEpsilon);
    EXPECT_NEAR(box.max().x(), 1.75, kEpsilon);
    EXPECT_NEAR(box.max().y(), -1.25, kEpsilon);
    EXPECT_NEAR(box.max().z(), -2.75, kEpsilon);
}

TEST(AabbTests, EmptyHitableListHasNoBoundingBox) {
    HitableList world;
    AABB box;

    EXPECT_FALSE(world.bounding_box(box));
}

TEST(AabbTests, HitableListBoundingBoxContainsAllObjects) {
    const auto material = std::make_shared<Lambertian>(Color(0.4, 0.5, 0.6));

    HitableList world;
    world.add(std::make_shared<Sphere>(Point3(-1.0, 0.0, -2.0), 0.5, material));
    world.add(std::make_shared<Sphere>(Point3(2.0, 1.0, -4.0), 1.0, material));

    AABB box;
    EXPECT_TRUE(world.bounding_box(box));
    EXPECT_NEAR(box.min().x(), -1.5, kEpsilon);
    EXPECT_NEAR(box.min().y(), -0.5, kEpsilon);
    EXPECT_NEAR(box.min().z(), -5.0, kEpsilon);
    EXPECT_NEAR(box.max().x(), 3.0, kEpsilon);
    EXPECT_NEAR(box.max().y(), 2.0, kEpsilon);
    EXPECT_NEAR(box.max().z(), -1.5, kEpsilon);
}
