#ifndef RAYTRACER_H
#define RAYTRACER_H

#include <cmath>
#include <iostream>
#include <vector>
#include <limits>
#include <memory>
#include <random>
#include <algorithm>
#include <stdexcept>
#include <atomic>
#include <thread>
#include <cstdint>
#include <functional>

// Constants and Utils
inline constexpr double infinity = std::numeric_limits<double>::infinity();
inline constexpr double pi = 3.1415926535897932385;

inline double degrees_to_radians(double degrees) {
    return degrees * pi / 180.0;
}

inline uint64_t splitmix64(uint64_t x) {
    x += 0x9e3779b97f4a7c15ULL;
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
    return x ^ (x >> 31);
}

inline uint64_t init_thread_rng_state() {
    static std::atomic<uint64_t> seed_counter{0x123456789abcdef0ULL};
    uint64_t seed = seed_counter.fetch_add(0x9e3779b97f4a7c15ULL, std::memory_order_relaxed);
    seed ^= static_cast<uint64_t>(std::hash<std::thread::id>{}(std::this_thread::get_id()));
    seed = splitmix64(seed);
    return seed == 0 ? 0x2545F4914F6CDD1DULL : seed;
}

inline uint64_t xorshift64star(uint64_t& state) {
    state ^= state >> 12;
    state ^= state << 25;
    state ^= state >> 27;
    return state * 0x2545F4914F6CDD1DULL;
}

inline double random_double() {
    static thread_local uint64_t state = init_thread_rng_state();
    const uint64_t r = xorshift64star(state);
    return static_cast<double>(r >> 11) * (1.0 / 9007199254740992.0);
}

inline double random_double(double min, double max) {
    return min + (max-min)*random_double();
}

inline double clamp(double x, double min, double max) {
    if (x < min) return min;
    if (x > max) return max;
    return x;
}

// Vec3 Class
class Vec3 {
public:
    double e[3];

    Vec3() : e{0,0,0} {}
    Vec3(double e0, double e1, double e2) : e{e0, e1, e2} {}

    double x() const { return e[0]; }
    double y() const { return e[1]; }
    double z() const { return e[2]; }

    Vec3 operator-() const { return Vec3(-e[0], -e[1], -e[2]); }
    double operator[](int i) const { return e[i]; }
    double& operator[](int i) { return e[i]; }

    Vec3& operator+=(const Vec3 &v) {
        e[0] += v.e[0];
        e[1] += v.e[1];
        e[2] += v.e[2];
        return *this;
    }

    Vec3& operator*=(const double t) {
        e[0] *= t;
        e[1] *= t;
        e[2] *= t;
        return *this;
    }

    Vec3& operator/=(const double t) {
        return *this *= 1/t;
    }

    double length() const { return std::sqrt(length_squared()); }
    double length_squared() const { return e[0]*e[0] + e[1]*e[1] + e[2]*e[2]; }

    static Vec3 random() {
        return Vec3(random_double(), random_double(), random_double());
    }

    static Vec3 random(double min, double max) {
        return Vec3(random_double(min,max), random_double(min,max), random_double(min,max));
    }
};

using Point3 = Vec3;   // 3D point
using Color = Vec3;    // RGB color

// Vec3 Utilities
inline std::ostream& operator<<(std::ostream &out, const Vec3 &v) {
    return out << v.e[0] << ' ' << v.e[1] << ' ' << v.e[2];
}

inline Vec3 operator+(const Vec3 &u, const Vec3 &v) {
    return Vec3(u.e[0] + v.e[0], u.e[1] + v.e[1], u.e[2] + v.e[2]);
}

inline Vec3 operator-(const Vec3 &u, const Vec3 &v) {
    return Vec3(u.e[0] - v.e[0], u.e[1] - v.e[1], u.e[2] - v.e[2]);
}

inline Vec3 operator*(const Vec3 &u, const Vec3 &v) {
    return Vec3(u.e[0] * v.e[0], u.e[1] * v.e[1], u.e[2] * v.e[2]);
}

inline Vec3 operator*(double t, const Vec3 &v) {
    return Vec3(t*v.e[0], t*v.e[1], t*v.e[2]);
}

inline Vec3 operator*(const Vec3 &v, double t) {
    return t * v;
}

inline Vec3 operator/(Vec3 v, double t) {
    return (1/t) * v;
}

inline double dot(const Vec3 &u, const Vec3 &v) {
    return u.e[0] * v.e[0] + u.e[1] * v.e[1] + u.e[2] * v.e[2];
}

inline Vec3 cross(const Vec3 &u, const Vec3 &v) {
    return Vec3(u.e[1] * v.e[2] - u.e[2] * v.e[1],
                u.e[2] * v.e[0] - u.e[0] * v.e[2],
                u.e[0] * v.e[1] - u.e[1] * v.e[0]);
}

inline Vec3 unit_vector(Vec3 v) {
    return v / v.length();
}

inline Vec3 random_in_unit_sphere() {
    while (true) {
        auto p = Vec3::random(-1,1);
        if (p.length_squared() >= 1) continue;
        return p;
    }
}

inline Vec3 random_in_unit_disk() {
    while (true) {
        auto p = Vec3(random_double(-1, 1), random_double(-1, 1), 0);
        if (p.length_squared() >= 1) continue;
        return p;
    }
}

inline Vec3 random_unit_vector() {
    return unit_vector(random_in_unit_sphere());
}

inline Vec3 reflect(const Vec3& v, const Vec3& n) {
    return v - 2*dot(v,n)*n;
}

inline Vec3 refract(const Vec3& uv, const Vec3& n, double etai_over_etat) {
    auto cos_theta = fmin(dot(-uv, n), 1.0);
    Vec3 r_out_perp =  etai_over_etat * (uv + cos_theta*n);
    Vec3 r_out_parallel = -sqrt(fabs(1.0 - r_out_perp.length_squared())) * n;
    return r_out_perp + r_out_parallel;
}

// Ray Class
class Ray {
public:
    Ray() {}
    Ray(const Point3& origin, const Vec3& direction)
        : orig(origin), dir(direction) {}

    Point3 origin() const  { return orig; }
    Vec3 direction() const { return dir; }

    Point3 at(double t) const {
        return orig + t*dir;
    }

public:
    Point3 orig;
    Vec3 dir;
};

// Material and Hitable Forward Declarations
class Material;

struct HitRecord {
    Point3 p;
    Vec3 normal;
    std::shared_ptr<Material> mat_ptr;
    double t;
    bool front_face;

    inline void set_face_normal(const Ray& r, const Vec3& outward_normal) {
        front_face = dot(r.direction(), outward_normal) < 0;
        normal = front_face ? outward_normal : -outward_normal;
    }
};

class AABB {
public:
    AABB() {}
    AABB(const Point3& a, const Point3& b) : minimum(a), maximum(b) {}

    const Point3& min() const { return minimum; }
    const Point3& max() const { return maximum; }

    bool hit(const Ray& r, double t_min, double t_max) const {
        for (int axis = 0; axis < 3; ++axis) {
            const double inv_d = 1.0 / r.direction()[axis];
            double t0 = (min()[axis] - r.origin()[axis]) * inv_d;
            double t1 = (max()[axis] - r.origin()[axis]) * inv_d;
            if (inv_d < 0.0) {
                std::swap(t0, t1);
            }
            t_min = t0 > t_min ? t0 : t_min;
            t_max = t1 < t_max ? t1 : t_max;
            if (t_max <= t_min) {
                return false;
            }
        }
        return true;
    }

private:
    Point3 minimum;
    Point3 maximum;
};

inline AABB surrounding_box(const AABB& box0, const AABB& box1) {
    Point3 small(
        std::fmin(box0.min().x(), box1.min().x()),
        std::fmin(box0.min().y(), box1.min().y()),
        std::fmin(box0.min().z(), box1.min().z())
    );

    Point3 big(
        std::fmax(box0.max().x(), box1.max().x()),
        std::fmax(box0.max().y(), box1.max().y()),
        std::fmax(box0.max().z(), box1.max().z())
    );

    return AABB(small, big);
}

class Hitable {
public:
    virtual bool hit(const Ray& r, double t_min, double t_max, HitRecord& rec) const = 0;
    virtual bool bounding_box(AABB& output_box) const = 0;
    virtual ~Hitable() = default;
};

class Sphere : public Hitable {
public:
    Sphere() {}
    Sphere(Point3 cen, double r, std::shared_ptr<Material> m)
        : center(cen), radius(r), mat_ptr(m) {};

    virtual bool hit(const Ray& r, double t_min, double t_max, HitRecord& rec) const override;
    virtual bool bounding_box(AABB& output_box) const override;

public:
    Point3 center;
    double radius;
    std::shared_ptr<Material> mat_ptr;
};

inline bool Sphere::hit(const Ray& r, double t_min, double t_max, HitRecord& rec) const {
    Vec3 oc = r.origin() - center;
    auto a = r.direction().length_squared();
    auto half_b = dot(oc, r.direction());
    auto c = oc.length_squared() - radius*radius;
    auto discriminant = half_b*half_b - a*c;

    if (discriminant < 0) return false;
    auto sqrtd = sqrt(discriminant);

    // Find the nearest root that lies in the acceptable range.
    auto root = (-half_b - sqrtd) / a;
    if (root < t_min || root > t_max) {
        root = (-half_b + sqrtd) / a;
        if (root < t_min || root > t_max)
            return false;
    }

    rec.t = root;
    rec.p = r.at(rec.t);
    Vec3 outward_normal = (rec.p - center) / radius;
    rec.set_face_normal(r, outward_normal);
    rec.mat_ptr = mat_ptr;

    return true;
}

inline bool Sphere::bounding_box(AABB& output_box) const {
    output_box = AABB(
        center - Vec3(radius, radius, radius),
        center + Vec3(radius, radius, radius)
    );
    return true;
}

class HitableList : public Hitable {
public:
    HitableList() {}
    HitableList(std::shared_ptr<Hitable> object) { add(object); }

    void clear() { objects.clear(); }
    void add(std::shared_ptr<Hitable> object) { objects.push_back(object); }

    virtual bool hit(const Ray& r, double t_min, double t_max, HitRecord& rec) const override;
    virtual bool bounding_box(AABB& output_box) const override;

public:
    std::vector<std::shared_ptr<Hitable>> objects;
};

inline bool HitableList::hit(const Ray& r, double t_min, double t_max, HitRecord& rec) const {
    HitRecord temp_rec;
    bool hit_anything = false;
    auto closest_so_far = t_max;

    for (const auto& object : objects) {
        if (object->hit(r, t_min, closest_so_far, temp_rec)) {
            hit_anything = true;
            closest_so_far = temp_rec.t;
            rec = temp_rec;
        }
    }

    return hit_anything;
}

inline bool HitableList::bounding_box(AABB& output_box) const {
    if (objects.empty()) {
        return false;
    }

    AABB temp_box;
    bool first_box = true;

    for (const auto& object : objects) {
        if (!object->bounding_box(temp_box)) {
            return false;
        }
        output_box = first_box ? temp_box : surrounding_box(output_box, temp_box);
        first_box = false;
    }

    return true;
}

class BVHNode : public Hitable {
public:
    BVHNode() {}
    BVHNode(std::vector<std::shared_ptr<Hitable>>& src_objects, size_t start, size_t end);

    bool hit(const Ray& r, double t_min, double t_max, HitRecord& rec) const override;
    bool bounding_box(AABB& output_box) const override;

private:
    std::shared_ptr<Hitable> left;
    std::shared_ptr<Hitable> right;
    AABB box;

    static bool box_compare(const std::shared_ptr<Hitable>& a, const std::shared_ptr<Hitable>& b, int axis);
    static bool box_x_compare(const std::shared_ptr<Hitable>& a, const std::shared_ptr<Hitable>& b);
    static bool box_y_compare(const std::shared_ptr<Hitable>& a, const std::shared_ptr<Hitable>& b);
    static bool box_z_compare(const std::shared_ptr<Hitable>& a, const std::shared_ptr<Hitable>& b);
};

inline BVHNode::BVHNode(std::vector<std::shared_ptr<Hitable>>& src_objects, size_t start, size_t end) {
    const int axis = static_cast<int>(3 * random_double());
    auto comparator = (axis == 0) ? box_x_compare : (axis == 1) ? box_y_compare : box_z_compare;
    const size_t object_span = end - start;

    if (object_span == 0) {
        throw std::invalid_argument("BVHNode requires at least one object.");
    }

    if (object_span == 1) {
        left = right = src_objects[start];
    } else if (object_span == 2) {
        if (comparator(src_objects[start], src_objects[start + 1])) {
            left = src_objects[start];
            right = src_objects[start + 1];
        } else {
            left = src_objects[start + 1];
            right = src_objects[start];
        }
    } else {
        std::sort(src_objects.begin() + static_cast<std::ptrdiff_t>(start),
                  src_objects.begin() + static_cast<std::ptrdiff_t>(end), comparator);

        const size_t mid = start + object_span / 2;
        left = std::make_shared<BVHNode>(src_objects, start, mid);
        right = std::make_shared<BVHNode>(src_objects, mid, end);
    }

    AABB box_left;
    AABB box_right;

    if (!left->bounding_box(box_left) || !right->bounding_box(box_right)) {
        throw std::runtime_error("No bounding box in BVHNode constructor.");
    }

    box = surrounding_box(box_left, box_right);
}

inline bool BVHNode::hit(const Ray& r, double t_min, double t_max, HitRecord& rec) const {
    if (!box.hit(r, t_min, t_max)) {
        return false;
    }

    const bool hit_left = left->hit(r, t_min, t_max, rec);
    const bool hit_right = right->hit(r, t_min, hit_left ? rec.t : t_max, rec);
    return hit_left || hit_right;
}

inline bool BVHNode::bounding_box(AABB& output_box) const {
    output_box = box;
    return true;
}

inline bool BVHNode::box_compare(const std::shared_ptr<Hitable>& a, const std::shared_ptr<Hitable>& b, int axis) {
    AABB box_a;
    AABB box_b;
    if (!a->bounding_box(box_a) || !b->bounding_box(box_b)) {
        throw std::runtime_error("No bounding box in BVHNode comparator.");
    }
    return box_a.min()[axis] < box_b.min()[axis];
}

inline bool BVHNode::box_x_compare(const std::shared_ptr<Hitable>& a, const std::shared_ptr<Hitable>& b) {
    return box_compare(a, b, 0);
}

inline bool BVHNode::box_y_compare(const std::shared_ptr<Hitable>& a, const std::shared_ptr<Hitable>& b) {
    return box_compare(a, b, 1);
}

inline bool BVHNode::box_z_compare(const std::shared_ptr<Hitable>& a, const std::shared_ptr<Hitable>& b) {
    return box_compare(a, b, 2);
}

// Materials
class Material {
public:
    virtual bool scatter(const Ray& r_in, const HitRecord& rec, Color& attenuation, Ray& scattered) const = 0;
};

class Lambertian : public Material {
public:
    Lambertian(const Color& a) : albedo(a) {}

    virtual bool scatter(const Ray& r_in, const HitRecord& rec, Color& attenuation, Ray& scattered) const override {
        auto scatter_direction = rec.normal + random_unit_vector();
        if (scatter_direction.length_squared() < 1e-8)
            scatter_direction = rec.normal;
        scattered = Ray(rec.p, scatter_direction);
        attenuation = albedo;
        return true;
    }

public:
    Color albedo;
};

class Metal : public Material {
public:
    Metal(const Color& a, double f) : albedo(a), fuzz(f < 1 ? f : 1) {}

    virtual bool scatter(const Ray& r_in, const HitRecord& rec, Color& attenuation, Ray& scattered) const override {
        Vec3 reflected = reflect(unit_vector(r_in.direction()), rec.normal);
        scattered = Ray(rec.p, reflected + fuzz*random_in_unit_sphere());
        attenuation = albedo;
        return (dot(scattered.direction(), rec.normal) > 0);
    }

public:
    Color albedo;
    double fuzz;
};

class Dielectric : public Material {
public:
    Dielectric(double index_of_refraction) : ir(index_of_refraction) {}

    virtual bool scatter(const Ray& r_in, const HitRecord& rec, Color& attenuation, Ray& scattered) const override {
        attenuation = Color(1.0, 1.0, 1.0);
        double refraction_ratio = rec.front_face ? (1.0/ir) : ir;

        Vec3 unit_direction = unit_vector(r_in.direction());
        double cos_theta = fmin(dot(-unit_direction, rec.normal), 1.0);
        double sin_theta = sqrt(1.0 - cos_theta*cos_theta);

        bool cannot_refract = refraction_ratio * sin_theta > 1.0;
        Vec3 direction;

        if (cannot_refract || reflectance(cos_theta, refraction_ratio) > random_double())
            direction = reflect(unit_direction, rec.normal);
        else
            direction = refract(unit_direction, rec.normal, refraction_ratio);

        scattered = Ray(rec.p, direction);
        return true;
    }

private:
    static double reflectance(double cosine, double ref_idx) {
        // Schlick's approximation
        auto r0 = (1-ref_idx) / (1+ref_idx);
        r0 = r0*r0;
        return r0 + (1-r0)*pow((1 - cosine),5);
    }

public:
    double ir; 
};

// Camera
class Camera {
public:
    Camera(Point3 lookfrom, Point3 lookat, Vec3 vup, double vfov, double aspect_ratio, double aperture, double focus_dist) {
        auto theta = degrees_to_radians(vfov);
        auto h = tan(theta/2);
        auto viewport_height = 2.0 * h;
        auto viewport_width = aspect_ratio * viewport_height;

        w = unit_vector(lookfrom - lookat);
        u = unit_vector(cross(vup, w));
        v = cross(w, u);

        origin = lookfrom;
        horizontal = focus_dist * viewport_width * u;
        vertical = focus_dist * viewport_height * v;
        lower_left_corner = origin - horizontal/2 - vertical/2 - focus_dist*w;

        lens_radius = aperture / 2;
    }

    Ray get_ray(double s, double t) const {
        const Vec3 rd_disk = lens_radius * random_in_unit_disk();
        Vec3 offset = u * rd_disk.x() + v * rd_disk.y();
        return Ray(origin + offset, lower_left_corner + s*horizontal + t*vertical - origin - offset);
    }

private:
    Point3 origin;
    Point3 lower_left_corner;
    Vec3 horizontal;
    Vec3 vertical;
    Vec3 u, v, w;
    double lens_radius;
};

// Color Function
inline Color ray_color(const Ray& r, const Hitable& world, int depth) {
    HitRecord rec;

    if (depth <= 0)
        return Color(0,0,0);

    if (world.hit(r, 0.001, infinity, rec)) {
        Ray scattered;
        Color attenuation;
        if (rec.mat_ptr->scatter(r, rec, attenuation, scattered))
            return attenuation * ray_color(scattered, world, depth-1);
        return Color(0,0,0);
    }

    Vec3 unit_direction = unit_vector(r.direction());
    auto t = 0.5*(unit_direction.y() + 1.0);
    return (1.0-t)*Color(1.0, 1.0, 1.0) + t*Color(0.5, 0.7, 1.0);
}

// Scene Helper
inline HitableList random_scene() {
    HitableList world;

    auto ground_material = std::make_shared<Lambertian>(Color(0.5, 0.5, 0.5));
    world.add(std::make_shared<Sphere>(Point3(0,-1000,0), 1000, ground_material));

    for (int a = -11; a < 11; a++) {
        for (int b = -11; b < 11; b++) {
            auto choose_mat = random_double();
            Point3 center(a + 0.9*random_double(), 0.2, b + 0.9*random_double());

            if ((center - Point3(4, 0.2, 0)).length() > 0.9) {
                std::shared_ptr<Material> sphere_material;

                if (choose_mat < 0.8) {
                    // diffuse
                    auto albedo = Color::random() * Color::random();
                    sphere_material = std::make_shared<Lambertian>(albedo);
                    world.add(std::make_shared<Sphere>(center, 0.2, sphere_material));
                } else if (choose_mat < 0.95) {
                    // metal
                    auto albedo = Color::random(0.5, 1);
                    auto fuzz = random_double(0, 0.5);
                    sphere_material = std::make_shared<Metal>(albedo, fuzz);
                    world.add(std::make_shared<Sphere>(center, 0.2, sphere_material));
                } else {
                    // glass
                    sphere_material = std::make_shared<Dielectric>(1.5);
                    world.add(std::make_shared<Sphere>(center, 0.2, sphere_material));
                }
            }
        }
    }

    auto material1 = std::make_shared<Dielectric>(1.5);
    world.add(std::make_shared<Sphere>(Point3(0, 1, 0), 1.0, material1));

    auto material2 = std::make_shared<Lambertian>(Color(0.4, 0.2, 0.1));
    world.add(std::make_shared<Sphere>(Point3(-4, 1, 0), 1.0, material2));

    auto material3 = std::make_shared<Metal>(Color(0.7, 0.6, 0.5), 0.0);
    world.add(std::make_shared<Sphere>(Point3(4, 1, 0), 1.0, material3));

    return world;
}

#endif // RAYTRACER_H
