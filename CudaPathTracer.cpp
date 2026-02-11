#include "CudaPathTracer.h"

#ifdef ENABLE_CUDA_BACKEND
extern "C" {
bool cudaPathTracerInit(int width, int height, const char **errorMessage);
bool cudaPathTracerRender(int frameIndex, int maxDepth, const unsigned int **hostPixels, const char **errorMessage);
void cudaPathTracerShutdown();
}
#endif

CudaPathTracer::CudaPathTracer() {
}

CudaPathTracer::~CudaPathTracer() {
#ifdef ENABLE_CUDA_BACKEND
    cudaPathTracerShutdown();
#endif
}

bool CudaPathTracer::initialize(int width, int height) {
    m_width = width;
    m_height = height;
    m_frameIndex = 0;
    m_hostPixels = nullptr;

#ifdef ENABLE_CUDA_BACKEND
    const char *err = nullptr;
    if (!cudaPathTracerInit(width, height, &err)) {
        m_lastError = err ? QString::fromUtf8(err) : QStringLiteral("CUDA initialization failed");
        return false;
    }
    return true;
#else
    Q_UNUSED(width)
    Q_UNUSED(height)
    m_lastError = QStringLiteral("CUDA backend is not enabled in this build");
    return false;
#endif
}

bool CudaPathTracer::renderFrame(int maxDepth) {
#ifdef ENABLE_CUDA_BACKEND
    const char *err = nullptr;
    const unsigned int *pixels = nullptr;
    if (!cudaPathTracerRender(m_frameIndex, maxDepth, &pixels, &err)) {
        m_lastError = err ? QString::fromUtf8(err) : QStringLiteral("CUDA render failed");
        return false;
    }
    m_hostPixels = pixels;
    ++m_frameIndex;
    return true;
#else
    Q_UNUSED(maxDepth)
    m_lastError = QStringLiteral("CUDA backend is not enabled in this build");
    return false;
#endif
}

const unsigned int *CudaPathTracer::hostPixels() const {
    return m_hostPixels;
}

int CudaPathTracer::frameIndex() const {
    return m_frameIndex;
}

QString CudaPathTracer::lastError() const {
    return m_lastError;
}
