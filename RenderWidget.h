#ifndef RENDERWIDGET_H
#define RENDERWIDGET_H

#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLTexture>
#include <QImage>
#include <QThread>
#include <QVector>
#include <QElapsedTimer>
#include <QString>
#include <atomic>
#include "RayTracer.h"

// Worker class to perform rendering in a separate thread
class RenderWorker : public QObject {
    Q_OBJECT
public:
    RenderWorker(int width, int height, int samples, int depth, QObject* parent = nullptr);
    void stop();

public slots:
    void render();

signals:
    void tileRendered(int line, int xStart, const QVector<unsigned int>& pixelData);
    void progressUpdated(int percentage);
    void finished();

private:
    int m_width;
    int m_height;
    int m_samples;
    int m_depth;
    std::atomic<bool> m_stop{false};
};

class RenderWidget : public QOpenGLWidget, protected QOpenGLFunctions {
    Q_OBJECT
public:
    explicit RenderWidget(QWidget *parent = nullptr);
    ~RenderWidget();

    void startRender();
    void stopRender();
    bool isRendering() const;
    
    // Settings
    void setResolution(int w, int h) { m_width = w; m_height = h; }
    void setSamples(int s) { m_samples = s; }
    void setDepth(int d) { m_depth = d; }

protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;

signals:
    void renderFinished();
    void progressChanged(int value);
    void renderStatsUpdated(const QString& statsText);

private slots:
    void updateTile(int line, int xStart, const QVector<unsigned int>& pixelData);
    void onWorkerFinished();

private:
    void ensureTexture();
    void destroyGlResources();

    QImage m_image;
    QThread* m_thread = nullptr;
    RenderWorker* m_worker = nullptr;
    int m_width = 800;
    int m_height = 450;
    int m_samples = 10;
    int m_depth = 10;
    bool m_isRendering = false;
    QElapsedTimer m_updateThrottleTimer;
    QElapsedTimer m_renderTimer;
    qint64 m_lastUpdateMs = 0;
    int m_repaintRequests = 0;

    std::unique_ptr<QOpenGLShaderProgram> m_program;
    std::unique_ptr<QOpenGLTexture> m_texture;
    bool m_textureDirty = true;
};

#endif // RENDERWIDGET_H
