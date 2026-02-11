#include <gtest/gtest.h>
#include "RayTracer.h"

namespace {
constexpr double kEpsilon = 1e-9;

void ExpectVec3Near(const Vec3& v, double x, double y, double z) {
    EXPECT_NEAR(v.x(), x, kEpsilon);
    EXPECT_NEAR(v.y(), y, kEpsilon);
    EXPECT_NEAR(v.z(), z, kEpsilon);
}
}

TEST(Vec3Tests, DefaultConstructorInitializesToZero) {
    const Vec3 v;
    ExpectVec3Near(v, 0.0, 0.0, 0.0);
}

TEST(Vec3Tests, AdditionAndSubtraction) {
    const Vec3 a(1.0, 2.0, 3.0);
    const Vec3 b(4.0, 5.0, 6.0);

    const Vec3 sum = a + b;
    const Vec3 diff = b - a;

    ExpectVec3Near(sum, 5.0, 7.0, 9.0);
    ExpectVec3Near(diff, 3.0, 3.0, 3.0);
}

TEST(Vec3Tests, DotAndCrossProduct) {
    const Vec3 a(1.0, 2.0, 3.0);
    const Vec3 b(4.0, 5.0, 6.0);

    const double dot_value = dot(a, b);
    const Vec3 cross_value = cross(a, b);

    EXPECT_NEAR(dot_value, 32.0, kEpsilon);
    ExpectVec3Near(cross_value, -3.0, 6.0, -3.0);
}

TEST(Vec3Tests, LengthAndLengthSquared) {
    const Vec3 v(3.0, 4.0, 12.0);

    const double length_value = v.length();
    const double length_squared_value = v.length_squared();

    EXPECT_NEAR(length_value, 13.0, kEpsilon);
    EXPECT_NEAR(length_squared_value, 169.0, kEpsilon);
}
