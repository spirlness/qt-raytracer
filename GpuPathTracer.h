#ifndef GPUPATHTRACER_H
#define GPUPATHTRACER_H

#include <QOpenGLExtraFunctions>
#include <QOpenGLShaderProgram>
#include <QString>
#include <memory>

class GpuPathTracer : protected QOpenGLExtraFunctions {
public:
    GpuPathTracer();
    ~GpuPathTracer();

    bool initialize();
    bool resize(int width, int height);
    bool renderFrame(int samplesPerFrame, int maxDepth);
    void resetAccumulation();
    bool isReady() const;
    GLuint outputTextureId() const;
    int width() const;
    int height() const;
    int frameIndex() const;
    QString lastError() const;

private:
    void release();
    bool ensureProgram();
    bool ensureTextures();

    std::unique_ptr<QOpenGLShaderProgram> m_program;
    GLuint m_accumTex = 0;
    GLuint m_outputTex = 0;
    int m_width = 0;
    int m_height = 0;
    int m_frameIndex = 0;
    bool m_ready = false;
    QString m_lastError;
};

#endif
