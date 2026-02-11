#ifdef ENABLE_CUDA_BACKEND

#include <cuda_runtime.h>

#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

namespace {

struct CudaState {
    int width = 0;
    int height = 0;
    float *dAccum = nullptr;
    unsigned int *dOutput = nullptr;
    std::vector<unsigned int> hostOutput;
    char error[256] = {0};
};

CudaState gState;

__device__ uint32_t hash32(uint32_t x) {
    x ^= x >> 16;
    x *= 0x7feb352du;
    x ^= x >> 15;
    x *= 0x846ca68bu;
    x ^= x >> 16;
    return x;
}

__device__ float rand01(uint32_t &state) {
    state = hash32(state);
    return static_cast<float>(state) / 4294967295.0f;
}

struct Vec3 {
    float x;
    float y;
    float z;
};

__device__ Vec3 make(float x, float y, float z) {
    return Vec3{x, y, z};
}

__device__ Vec3 add(const Vec3 &a, const Vec3 &b) { return make(a.x + b.x, a.y + b.y, a.z + b.z); }
__device__ Vec3 sub(const Vec3 &a, const Vec3 &b) { return make(a.x - b.x, a.y - b.y, a.z - b.z); }
__device__ Vec3 mul(const Vec3 &a, float s) { return make(a.x * s, a.y * s, a.z * s); }
__device__ Vec3 mulv(const Vec3 &a, const Vec3 &b) { return make(a.x * b.x, a.y * b.y, a.z * b.z); }
__device__ float dot(const Vec3 &a, const Vec3 &b) { return a.x * b.x + a.y * b.y + a.z * b.z; }

__device__ Vec3 normalize(const Vec3 &v) {
    float len = sqrtf(dot(v, v));
    if (len <= 1e-6f) {
        return make(0.0f, 0.0f, 0.0f);
    }
    return make(v.x / len, v.y / len, v.z / len);
}

__device__ Vec3 randomInUnitSphere(uint32_t &state) {
    while (true) {
        Vec3 p = make(rand01(state) * 2.0f - 1.0f, rand01(state) * 2.0f - 1.0f, rand01(state) * 2.0f - 1.0f);
        if (dot(p, p) < 1.0f) {
            return p;
        }
    }
}

__device__ bool hitSphere(const Vec3 &center, float radius, const Vec3 &ro, const Vec3 &rd, float &t, Vec3 &n, Vec3 &albedo) {
    Vec3 oc = sub(ro, center);
    float a = dot(rd, rd);
    float b = dot(oc, rd);
    float c = dot(oc, oc) - radius * radius;
    float d = b * b - a * c;
    if (d < 0.0f) {
        return false;
    }
    float s = sqrtf(d);
    float t0 = (-b - s) / a;
    float t1 = (-b + s) / a;
    t = t0 > 0.001f ? t0 : t1;
    if (t <= 0.001f) {
        return false;
    }
    Vec3 p = add(ro, mul(rd, t));
    n = normalize(sub(p, center));
    if (radius > 50.0f) {
        albedo = make(0.8f, 0.8f, 0.0f);
    } else if (center.x < -0.5f) {
        albedo = make(0.8f, 0.3f, 0.3f);
    } else if (center.x > 0.5f) {
        albedo = make(0.3f, 0.8f, 0.3f);
    } else {
        albedo = make(0.75f, 0.75f, 0.75f);
    }
    return true;
}

__device__ Vec3 traceRay(Vec3 ro, Vec3 rd, uint32_t &state, int maxDepth) {
    Vec3 throughput = make(1.0f, 1.0f, 1.0f);
    Vec3 radiance = make(0.0f, 0.0f, 0.0f);

    for (int depth = 0; depth < maxDepth; ++depth) {
        float bestT = 1e20f;
        Vec3 bestN = make(0.0f, 0.0f, 0.0f);
        Vec3 bestAlbedo = make(0.0f, 0.0f, 0.0f);
        bool hit = false;

        float t;
        Vec3 n;
        Vec3 albedo;
        if (hitSphere(make(0.0f, -100.5f, -1.0f), 100.0f, ro, rd, t, n, albedo) && t < bestT) { bestT = t; bestN = n; bestAlbedo = albedo; hit = true; }
        if (hitSphere(make(0.0f, 0.0f, -1.0f), 0.5f, ro, rd, t, n, albedo) && t < bestT) { bestT = t; bestN = n; bestAlbedo = albedo; hit = true; }
        if (hitSphere(make(-1.0f, 0.0f, -1.4f), 0.5f, ro, rd, t, n, albedo) && t < bestT) { bestT = t; bestN = n; bestAlbedo = albedo; hit = true; }
        if (hitSphere(make(1.0f, 0.0f, -1.2f), 0.5f, ro, rd, t, n, albedo) && t < bestT) { bestT = t; bestN = n; bestAlbedo = albedo; hit = true; }

        if (!hit) {
            Vec3 unit = normalize(rd);
            float a = 0.5f * (unit.y + 1.0f);
            Vec3 sky = add(mul(make(1.0f, 1.0f, 1.0f), 1.0f - a), mul(make(0.5f, 0.7f, 1.0f), a));
            radiance = add(radiance, mulv(throughput, sky));
            break;
        }

        Vec3 hitPos = add(ro, mul(rd, bestT));
        Vec3 scatterDir = normalize(add(bestN, randomInUnitSphere(state)));
        ro = add(hitPos, mul(bestN, 0.001f));
        rd = scatterDir;
        throughput = mulv(throughput, bestAlbedo);
    }

    return radiance;
}

__global__ void pathTraceKernel(float *accum, unsigned int *output, int width, int height, int frameIndex, int maxDepth) {
    const int x = blockIdx.x * blockDim.x + threadIdx.x;
    const int y = blockIdx.y * blockDim.y + threadIdx.y;
    if (x >= width || y >= height) {
        return;
    }

    const int idx = y * width + x;
    uint32_t seed = static_cast<uint32_t>((x + y * width) * 9781 + (frameIndex + 1) * 6271);
    float u = (static_cast<float>(x) + rand01(seed)) / static_cast<float>(max(1, width - 1));
    float v = (static_cast<float>(y) + rand01(seed)) / static_cast<float>(max(1, height - 1));

    float aspect = static_cast<float>(width) / static_cast<float>(height);
    Vec3 origin = make(0.0f, 0.3f, 1.2f);
    Vec3 lowerLeft = make(-aspect, -1.0f, -1.0f);
    Vec3 horizontal = make(2.0f * aspect, 0.0f, 0.0f);
    Vec3 vertical = make(0.0f, 2.0f, 0.0f);
    Vec3 rd = normalize(sub(add(add(lowerLeft, mul(horizontal, u)), mul(vertical, v)), origin));

    Vec3 sample = traceRay(origin, rd, seed, maxDepth);

    float prevR = accum[idx * 4 + 0];
    float prevG = accum[idx * 4 + 1];
    float prevB = accum[idx * 4 + 2];
    float frameCount = static_cast<float>(frameIndex + 1);

    float outR = (prevR * static_cast<float>(frameIndex) + sample.x) / frameCount;
    float outG = (prevG * static_cast<float>(frameIndex) + sample.y) / frameCount;
    float outB = (prevB * static_cast<float>(frameIndex) + sample.z) / frameCount;

    accum[idx * 4 + 0] = outR;
    accum[idx * 4 + 1] = outG;
    accum[idx * 4 + 2] = outB;
    accum[idx * 4 + 3] = 1.0f;

    outR = sqrtf(fminf(fmaxf(outR, 0.0f), 1.0f));
    outG = sqrtf(fminf(fmaxf(outG, 0.0f), 1.0f));
    outB = sqrtf(fminf(fmaxf(outB, 0.0f), 1.0f));

    unsigned int ir = static_cast<unsigned int>(outR * 255.0f);
    unsigned int ig = static_cast<unsigned int>(outG * 255.0f);
    unsigned int ib = static_cast<unsigned int>(outB * 255.0f);
    output[idx] = (255u << 24) | (ir << 16) | (ig << 8) | ib;
}

void setError(const char *msg) {
    std::strncpy(gState.error, msg, sizeof(gState.error) - 1);
    gState.error[sizeof(gState.error) - 1] = '\0';
}

}

extern "C" bool cudaPathTracerInit(int width, int height, const char **errorMessage) {
    if (gState.dAccum) {
        cudaFree(gState.dAccum);
        gState.dAccum = nullptr;
    }
    if (gState.dOutput) {
        cudaFree(gState.dOutput);
        gState.dOutput = nullptr;
    }

    gState.width = width;
    gState.height = height;
    gState.hostOutput.assign(static_cast<size_t>(width) * static_cast<size_t>(height), 0u);

    auto cleanupAllocations = []() {
        if (gState.dAccum) {
            cudaFree(gState.dAccum);
            gState.dAccum = nullptr;
        }
        if (gState.dOutput) {
            cudaFree(gState.dOutput);
            gState.dOutput = nullptr;
        }
    };

    cudaError_t err = cudaMalloc(reinterpret_cast<void **>(&gState.dAccum), static_cast<size_t>(width) * static_cast<size_t>(height) * 4 * sizeof(float));
    if (err != cudaSuccess) {
        setError(cudaGetErrorString(err));
        if (errorMessage) *errorMessage = gState.error;
        return false;
    }

    err = cudaMalloc(reinterpret_cast<void **>(&gState.dOutput), static_cast<size_t>(width) * static_cast<size_t>(height) * sizeof(unsigned int));
    if (err != cudaSuccess) {
        cleanupAllocations();
        setError(cudaGetErrorString(err));
        if (errorMessage) *errorMessage = gState.error;
        return false;
    }

    err = cudaMemset(gState.dAccum, 0, static_cast<size_t>(width) * static_cast<size_t>(height) * 4 * sizeof(float));
    if (err != cudaSuccess) {
        cleanupAllocations();
        setError(cudaGetErrorString(err));
        if (errorMessage) *errorMessage = gState.error;
        return false;
    }

    return true;
}

extern "C" bool cudaPathTracerRender(int frameIndex, int maxDepth, const unsigned int **hostPixels, const char **errorMessage) {
    if (!gState.dAccum || !gState.dOutput) {
        setError("CUDA path tracer not initialized");
        if (errorMessage) *errorMessage = gState.error;
        return false;
    }

    dim3 block(8, 8, 1);
    dim3 grid(
        static_cast<unsigned int>((gState.width + block.x - 1) / block.x),
        static_cast<unsigned int>((gState.height + block.y - 1) / block.y),
        1);

    pathTraceKernel<<<grid, block>>>(gState.dAccum, gState.dOutput, gState.width, gState.height, frameIndex, maxDepth);

    cudaError_t err = cudaGetLastError();
    if (err != cudaSuccess) {
        setError(cudaGetErrorString(err));
        if (errorMessage) *errorMessage = gState.error;
        return false;
    }

    err = cudaMemcpy(
        gState.hostOutput.data(),
        gState.dOutput,
        static_cast<size_t>(gState.width) * static_cast<size_t>(gState.height) * sizeof(unsigned int),
        cudaMemcpyDeviceToHost);
    if (err != cudaSuccess) {
        setError(cudaGetErrorString(err));
        if (errorMessage) *errorMessage = gState.error;
        return false;
    }

    if (hostPixels) {
        *hostPixels = gState.hostOutput.data();
    }
    return true;
}

extern "C" void cudaPathTracerShutdown() {
    if (gState.dAccum) {
        cudaFree(gState.dAccum);
        gState.dAccum = nullptr;
    }
    if (gState.dOutput) {
        cudaFree(gState.dOutput);
        gState.dOutput = nullptr;
    }
    gState.hostOutput.clear();
}

#endif
