#include <gtest/gtest.h>

#include "raytracer/RayTracer.h"

namespace {
constexpr double kEpsilon = 1e-9;
}

TEST(MaterialTests, LambertianScatterSetsAlbedoAndHitOrigin) {
    const Lambertian material(Color(0.2, 0.4, 0.6));

    HitRecord rec;
    rec.p = Point3(1.0, 2.0, 3.0);
    rec.normal = Vec3(0.0, 1.0, 0.0);
    rec.front_face = true;

    const Ray incoming(Point3(1.0, 3.0, 3.0), Vec3(0.0, -1.0, 0.0));
    Color attenuation;
    Ray scattered;

    EXPECT_TRUE(material.scatter(incoming, rec, attenuation, scattered));
    EXPECT_NEAR(attenuation.x(), 0.2, kEpsilon);
    EXPECT_NEAR(attenuation.y(), 0.4, kEpsilon);
    EXPECT_NEAR(attenuation.z(), 0.6, kEpsilon);
    EXPECT_NEAR(scattered.origin().x(), rec.p.x(), kEpsilon);
    EXPECT_NEAR(scattered.origin().y(), rec.p.y(), kEpsilon);
    EXPECT_NEAR(scattered.origin().z(), rec.p.z(), kEpsilon);
    EXPECT_GT(scattered.direction().length_squared(), 0.0);
}

TEST(MaterialTests, MetalWithZeroFuzzReflectsPerfectly) {
    const Metal material(Color(0.9, 0.9, 0.9), 0.0);

    HitRecord rec;
    rec.p = Point3(0.0, 0.0, 0.0);
    rec.normal = Vec3(0.0, 1.0, 0.0);
    rec.front_face = true;

    const Ray incoming(Point3(0.0, 1.0, 0.0), Vec3(0.0, -1.0, 0.0));
    Color attenuation;
    Ray scattered;

    EXPECT_TRUE(material.scatter(incoming, rec, attenuation, scattered));
    EXPECT_NEAR(scattered.direction().x(), 0.0, kEpsilon);
    EXPECT_NEAR(scattered.direction().y(), 1.0, kEpsilon);
    EXPECT_NEAR(scattered.direction().z(), 0.0, kEpsilon);
}

TEST(MaterialTests, DielectricScatterReturnsWhiteAttenuationAndValidDirection) {
    const Dielectric material(1.5);

    HitRecord rec;
    rec.p = Point3(0.0, 0.0, 0.0);
    rec.normal = Vec3(0.0, 1.0, 0.0);
    rec.front_face = true;

    const Ray incoming(Point3(0.0, 1.0, 0.0), Vec3(0.0, -1.0, 0.0));
    Color attenuation;
    Ray scattered;

    EXPECT_TRUE(material.scatter(incoming, rec, attenuation, scattered));
    EXPECT_NEAR(attenuation.x(), 1.0, kEpsilon);
    EXPECT_NEAR(attenuation.y(), 1.0, kEpsilon);
    EXPECT_NEAR(attenuation.z(), 1.0, kEpsilon);
    EXPECT_GT(scattered.direction().length_squared(), 0.0);
}
