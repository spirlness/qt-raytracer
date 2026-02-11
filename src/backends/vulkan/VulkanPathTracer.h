#ifndef VULKANPATHTRACER_H
#define VULKANPATHTRACER_H

#include <QString>
#include <vector>

class VulkanPathTracer {
public:
    VulkanPathTracer();
    ~VulkanPathTracer();

    bool initialize(int width, int height);
    bool renderFrame(int maxDepth);
    const unsigned int *hostPixels() const;
    int frameIndex() const;
    QString lastError() const;

private:
    bool initializeInternal(int width, int height);
    bool createPipeline();
    bool createImagesAndBuffers();
    bool recordAndSubmitInitClear();
    void cleanup();

    int m_width = 0;
    int m_height = 0;
    int m_frameIndex = 0;
    QString m_lastError;
    std::vector<unsigned int> m_hostOutput;

    struct Impl;
    Impl *m_impl = nullptr;
};

#endif
