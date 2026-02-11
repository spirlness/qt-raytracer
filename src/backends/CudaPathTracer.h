#ifndef CUDAPATHTRACER_H
#define CUDAPATHTRACER_H

#include <QString>

class CudaPathTracer {
public:
    CudaPathTracer();
    ~CudaPathTracer();

    bool initialize(int width, int height);
    bool renderFrame(int maxDepth);
    const unsigned int *hostPixels() const;
    int frameIndex() const;
    QString lastError() const;

private:
    int m_width = 0;
    int m_height = 0;
    int m_frameIndex = 0;
    const unsigned int *m_hostPixels = nullptr;
    QString m_lastError;
};

#endif
