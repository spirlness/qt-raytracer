#include <gtest/gtest.h>

#include <memory>
#include <vector>

#include "raytracer/RayTracer.h"

namespace {
constexpr double kEpsilon = 1e-9;
}

TEST(BvhTests, BoundingBoxContainsAllChildren) {
    const auto material = std::make_shared<Lambertian>(Color(0.7, 0.7, 0.7));
    std::vector<std::shared_ptr<Hitable>> objects;
    objects.push_back(std::make_shared<Sphere>(Point3(-2.0, 0.0, -1.0), 0.5, material));
    objects.push_back(std::make_shared<Sphere>(Point3(2.0, 1.0, -3.0), 1.0, material));
    objects.push_back(std::make_shared<Sphere>(Point3(0.0, -1.0, -2.0), 0.25, material));

    BVHNode bvh(objects, 0, objects.size());

    AABB box;
    EXPECT_TRUE(bvh.bounding_box(box));
    EXPECT_NEAR(box.min().x(), -2.5, kEpsilon);
    EXPECT_NEAR(box.min().y(), -1.25, kEpsilon);
    EXPECT_NEAR(box.min().z(), -4.0, kEpsilon);
    EXPECT_NEAR(box.max().x(), 3.0, kEpsilon);
    EXPECT_NEAR(box.max().y(), 2.0, kEpsilon);
    EXPECT_NEAR(box.max().z(), -0.5, kEpsilon);
}

TEST(BvhTests, HitFindsNearestObject) {
    const auto material = std::make_shared<Lambertian>(Color(0.7, 0.7, 0.7));
    std::vector<std::shared_ptr<Hitable>> objects;
    objects.push_back(std::make_shared<Sphere>(Point3(0.0, 0.0, -1.0), 0.5, material));
    objects.push_back(std::make_shared<Sphere>(Point3(0.0, 0.0, -3.0), 0.5, material));

    BVHNode bvh(objects, 0, objects.size());

    const Ray ray(Point3(0.0, 0.0, 0.0), Vec3(0.0, 0.0, -1.0));
    HitRecord rec;

    EXPECT_TRUE(bvh.hit(ray, 0.001, infinity, rec));
    EXPECT_NEAR(rec.t, 0.5, kEpsilon);
}

TEST(BvhTests, MissReturnsFalse) {
    const auto material = std::make_shared<Lambertian>(Color(0.7, 0.7, 0.7));
    std::vector<std::shared_ptr<Hitable>> objects;
    objects.push_back(std::make_shared<Sphere>(Point3(0.0, 0.0, -1.0), 0.5, material));
    objects.push_back(std::make_shared<Sphere>(Point3(0.0, 0.0, -3.0), 0.5, material));

    BVHNode bvh(objects, 0, objects.size());

    const Ray ray(Point3(0.0, 2.0, 0.0), Vec3(0.0, 0.0, -1.0));
    HitRecord rec;

    EXPECT_FALSE(bvh.hit(ray, 0.001, infinity, rec));
}
