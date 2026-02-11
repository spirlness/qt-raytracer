#ifndef TEST_HELPERS_H
#define TEST_HELPERS_H

#include <memory>
#include "RayTracer.h"

class TestMaterial final : public Material {
public:
    bool scatter(const Ray&, const HitRecord&, Color&, Ray&) const override {
        return false;
    }
};

#endif // TEST_HELPERS_H
