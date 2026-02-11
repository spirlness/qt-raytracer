#ifndef RAYTRACERFBOITEM_H
#define RAYTRACERFBOITEM_H

#include <QElapsedTimer>
#include <QImage>
#include <QMutex>
#include <QQuickItem>
#include <QSGRendererInterface>
#include <QThread>
#include <QVector>
#include <atomic>
#include <memory>

#include "RayTracer.h"

class RenderWorker : public QObject {
    Q_OBJECT
public:
    RenderWorker(int width, int height, int samples, int depth, int tileSize, QObject *parent = nullptr);
    void stop();

public slots:
    void render();

signals:
    void tileRendered(int yStart, int xStart, int tileWidth, int tileHeight, const QVector<unsigned int> &pixelData);
    void progressUpdated(int percentage);
    void finished();

private:
    int m_width;
    int m_height;
    int m_samples;
    int m_depth;
    int m_tileSize;
    std::atomic<bool> m_stop{false};
};

class QSGNode;
class GpuPathTracer;
class CudaPathTracer;
class VulkanPathTracer;

class RayTracerFboItem : public QQuickItem {
    Q_OBJECT
    Q_PROPERTY(int renderWidth READ renderWidth WRITE setRenderWidth NOTIFY renderWidthChanged)
    Q_PROPERTY(int renderHeight READ renderHeight WRITE setRenderHeight NOTIFY renderHeightChanged)
    Q_PROPERTY(int samples READ samples WRITE setSamples NOTIFY samplesChanged)
    Q_PROPERTY(int maxDepth READ maxDepth WRITE setMaxDepth NOTIFY maxDepthChanged)
    Q_PROPERTY(QString computeBackend READ computeBackend WRITE setComputeBackend NOTIFY computeBackendChanged)
    Q_PROPERTY(int progress READ progress NOTIFY progressChanged)
    Q_PROPERTY(bool rendering READ rendering NOTIFY renderingChanged)
    Q_PROPERTY(QString statsText READ statsText NOTIFY statsTextChanged)

public:
    explicit RayTracerFboItem(QQuickItem *parent = nullptr);
    ~RayTracerFboItem() override;

    int renderWidth() const;
    int renderHeight() const;
    int samples() const;
    int maxDepth() const;
    QString computeBackend() const;
    int progress() const;
    bool rendering() const;
    QString statsText() const;

    void setRenderWidth(int value);
    void setRenderHeight(int value);
    void setSamples(int value);
    void setMaxDepth(int value);
    void setComputeBackend(const QString &value);

    Q_INVOKABLE void startRender();
    Q_INVOKABLE void stopRender();

signals:
    void renderWidthChanged();
    void renderHeightChanged();
    void samplesChanged();
    void maxDepthChanged();
    void computeBackendChanged();
    void progressChanged();
    void renderingChanged();
    void statsTextChanged();

private slots:
    void onTileRendered(int yStart, int xStart, int tileWidth, int tileHeight, const QVector<unsigned int> &pixelData);
    void onWorkerProgressUpdated(int value);
    void onWorkerFinished();

protected:
    QSGNode *updatePaintNode(QSGNode *oldNode, QQuickItem::UpdatePaintNodeData *data) override;
    void releaseResources() override;

private:
    struct DirtyUpload {
        int yStart = 0;
        int xStart = 0;
        int width = 0;
        int height = 0;
        QVector<unsigned int> pixels;
    };

    void setRendering(bool value);
    void setProgress(int value);
    void setStatsText(const QString &value);
    int chooseTileSize(QSGRendererInterface::GraphicsApi api, int width, int height) const;
    int chooseMaxUploadsPerFrame(QSGRendererInterface::GraphicsApi api, int width, int height) const;

    int m_renderWidth = 800;
    int m_renderHeight = 450;
    int m_samples = 10;
    int m_maxDepth = 10;
    QString m_computeBackend = QStringLiteral("auto");
    int m_progress = 0;
    bool m_rendering = false;
    QString m_statsText = QStringLiteral("Last render: N/A");

    QImage m_image;
    mutable QMutex m_mutex;
    QVector<DirtyUpload> m_pendingUploads;
    bool m_fullUploadNeeded = true;

    QThread *m_thread = nullptr;
    RenderWorker *m_worker = nullptr;
    QElapsedTimer m_renderTimer;
    int m_repaintRequests = 0;
    int m_tileSize = 16;
    int m_maxUploadsPerFrame = 32;
    std::atomic<quint64> m_gpuUploadCalls{0};
    std::atomic<quint64> m_gpuUploadPixels{0};
    std::atomic<quint64> m_gpuUploadFrames{0};
    std::unique_ptr<GpuPathTracer> m_gpuTracer;
    std::unique_ptr<CudaPathTracer> m_cudaTracer;
    std::unique_ptr<VulkanPathTracer> m_vulkanTracer;
    std::atomic<bool> m_gpuModeActive{false};
    QString m_activeComputeKernel;
};

#endif
