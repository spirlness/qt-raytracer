#include "RayTracerFboItem.h"
#include "CudaPathTracer.h"
#include "GpuPathTracer.h"
#include "VulkanPathTracer.h"

#include <QMutexLocker>
#include <QMetaObject>
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QSGRendererInterface>
#include <QSGSimpleTextureNode>
#include <QSGTexture>
#include <algorithm>
#include <cstring>
#include <thread>

#if QT_CONFIG(opengl)
#include <QtQuick/qsgtexture_platform.h>
#endif

namespace {

class RayTracerTextureNode final : public QSGSimpleTextureNode {
public:
    RayTracerTextureNode() {
        setOwnsTexture(true);
    }
};

}

RenderWorker::RenderWorker(int width, int height, int samples, int depth, int tileSize, QObject *parent)
    : QObject(parent),
      m_width(width),
      m_height(height),
      m_samples(samples),
      m_depth(depth),
      m_tileSize(std::max(8, tileSize)) {
}

void RenderWorker::stop() {
    m_stop.store(true, std::memory_order_relaxed);
}

void RenderWorker::render() {
    m_stop.store(false, std::memory_order_relaxed);

    const auto aspectRatio = static_cast<double>(m_width) / static_cast<double>(m_height);
    Point3 lookfrom(13, 2, 3);
    Point3 lookat(0, 0, 0);
    Vec3 vup(0, 1, 0);
    const auto distToFocus = 10.0;
    const auto aperture = 0.1;

    Camera cam(lookfrom, lookat, vup, 20, aspectRatio, aperture, distToFocus);
    HitableList worldList = random_scene();
    std::vector<std::shared_ptr<Hitable>> worldObjects = worldList.objects;
    BVHNode world(worldObjects, 0, worldObjects.size());

    const int widthDenom = std::max(1, m_width - 1);
    const int heightDenom = std::max(1, m_height - 1);
    const double invWidthDenom = 1.0 / static_cast<double>(widthDenom);
    const double invHeightDenom = 1.0 / static_cast<double>(heightDenom);
    const double scale = 1.0 / static_cast<double>(m_samples);

    const int tileSize = m_tileSize;
    const int tilesX = (m_width + tileSize - 1) / tileSize;
    const int tilesY = (m_height + tileSize - 1) / tileSize;
    const int totalTiles = tilesX * tilesY;

    std::atomic<int> nextTile(0);
    std::atomic<int> completedTiles(0);

    int threadCount = static_cast<int>(std::thread::hardware_concurrency());
    if (threadCount <= 0) {
        threadCount = 1;
    }

    std::vector<std::thread> workers;
    workers.reserve(threadCount);

    for (int t = 0; t < threadCount; ++t) {
        workers.emplace_back([&]() {
            while (!m_stop.load(std::memory_order_relaxed)) {
                const int tileIndex = nextTile.fetch_add(1, std::memory_order_relaxed);
                if (tileIndex >= totalTiles) {
                    break;
                }

                const int tileX = tileIndex % tilesX;
                const int tileY = tileIndex / tilesX;
                const int xStart = tileX * tileSize;
                const int yStart = tileY * tileSize;
                const int xEnd = std::min(xStart + tileSize, m_width);
                const int yEnd = std::min(yStart + tileSize, m_height);
                const int tileWidth = xEnd - xStart;
                const int tileHeight = yEnd - yStart;

                QVector<unsigned int> tileData(tileWidth * tileHeight);

                for (int line = yStart; line < yEnd; ++line) {
                    const int j = m_height - 1 - line;
                    const int tileRow = line - yStart;

                    for (int i = xStart; i < xEnd; ++i) {
                        Color pixelColor(0, 0, 0);
                        for (int s = 0; s < m_samples; ++s) {
                            const double u = (static_cast<double>(i) + random_double()) * invWidthDenom;
                            const double v = (static_cast<double>(j) + random_double()) * invHeightDenom;
                            Ray r = cam.get_ray(u, v);
                            pixelColor += ray_color(r, world, m_depth);
                        }

                        const double r = std::sqrt(scale * pixelColor.x());
                        const double g = std::sqrt(scale * pixelColor.y());
                        const double b = std::sqrt(scale * pixelColor.z());

                        const int ir = static_cast<int>(256 * clamp(r, 0.0, 0.999));
                        const int ig = static_cast<int>(256 * clamp(g, 0.0, 0.999));
                        const int ib = static_cast<int>(256 * clamp(b, 0.0, 0.999));

                        tileData[tileRow * tileWidth + (i - xStart)] =
                            (255u << 24) |
                            (static_cast<unsigned int>(ir) << 16) |
                            (static_cast<unsigned int>(ig) << 8) |
                            static_cast<unsigned int>(ib);
                    }
                }

                emit tileRendered(yStart, xStart, tileWidth, tileHeight, tileData);

                const int done = completedTiles.fetch_add(1, std::memory_order_relaxed) + 1;
                emit progressUpdated(static_cast<int>((100.0 * done) / totalTiles));
            }
        });
    }

    for (std::thread &worker : workers) {
        worker.join();
    }

    emit finished();
}

RayTracerFboItem::RayTracerFboItem(QQuickItem *parent)
    : QQuickItem(parent) {
    setFlag(ItemHasContents, true);
    m_image = QImage(m_renderWidth, m_renderHeight, QImage::Format_ARGB32);
    m_image.fill(Qt::black);
}

RayTracerFboItem::~RayTracerFboItem() {
    stopRender();
}

int RayTracerFboItem::renderWidth() const {
    return m_renderWidth;
}

int RayTracerFboItem::renderHeight() const {
    return m_renderHeight;
}

int RayTracerFboItem::samples() const {
    return m_samples;
}

int RayTracerFboItem::maxDepth() const {
    return m_maxDepth;
}

int RayTracerFboItem::progress() const {
    return m_progress;
}

bool RayTracerFboItem::rendering() const {
    return m_rendering;
}

QString RayTracerFboItem::statsText() const {
    return m_statsText;
}

QString RayTracerFboItem::computeBackend() const {
    return m_computeBackend;
}

void RayTracerFboItem::setRenderWidth(int value) {
    if (m_renderWidth == value) {
        return;
    }
    m_renderWidth = std::max(64, value);
    emit renderWidthChanged();
}

void RayTracerFboItem::setRenderHeight(int value) {
    if (m_renderHeight == value) {
        return;
    }
    m_renderHeight = std::max(64, value);
    emit renderHeightChanged();
}

void RayTracerFboItem::setSamples(int value) {
    if (m_samples == value) {
        return;
    }
    m_samples = std::max(1, value);
    emit samplesChanged();
}

void RayTracerFboItem::setMaxDepth(int value) {
    if (m_maxDepth == value) {
        return;
    }
    m_maxDepth = std::max(1, value);
    emit maxDepthChanged();
}

void RayTracerFboItem::setComputeBackend(const QString &value) {
    const QString normalized = value.trimmed().toLower();
    if (normalized.isEmpty() || normalized == m_computeBackend) {
        return;
    }
    m_computeBackend = normalized;
    emit computeBackendChanged();
}

void RayTracerFboItem::startRender() {
    if (m_rendering) {
        return;
    }

    const auto api = window() && window()->rendererInterface()
        ? window()->rendererInterface()->graphicsApi()
        : QSGRendererInterface::Unknown;
    const bool isOpenGl = api == QSGRendererInterface::OpenGL;
    const QString backend = m_computeBackend.trimmed().toLower();
    const bool wantsOpenGlCompute = (backend == QStringLiteral("auto") || backend == QStringLiteral("opengl"));
    const bool wantsCudaCompute = (backend == QStringLiteral("cuda"));
    const bool wantsVulkanCompute = (backend == QStringLiteral("vulkan"));

    m_gpuModeActive.store(false, std::memory_order_relaxed);
    m_activeComputeKernel.clear();

    if (wantsOpenGlCompute && isOpenGl) {
        m_gpuModeActive.store(true, std::memory_order_relaxed);
        m_activeComputeKernel = QStringLiteral("opengl");
        if (!m_gpuTracer) {
            m_gpuTracer = std::make_unique<GpuPathTracer>();
        }
    }

    if (wantsCudaCompute) {
        m_gpuModeActive.store(true, std::memory_order_relaxed);
        m_activeComputeKernel = QStringLiteral("cuda");
        if (!m_cudaTracer) {
            m_cudaTracer = std::make_unique<CudaPathTracer>();
        }
        if (!m_cudaTracer->initialize(m_renderWidth, m_renderHeight)) {
            setStatsText(QStringLiteral("CUDA backend unavailable: %1. Falling back to CPU path tracer.")
                             .arg(m_cudaTracer->lastError()));
            m_gpuModeActive.store(false, std::memory_order_relaxed);
            m_activeComputeKernel.clear();
        }
    }

    if (wantsVulkanCompute) {
        m_gpuModeActive.store(true, std::memory_order_relaxed);
        m_activeComputeKernel = QStringLiteral("vulkan");
        if (!m_vulkanTracer) {
            m_vulkanTracer = std::make_unique<VulkanPathTracer>();
        }
        if (!m_vulkanTracer->initialize(m_renderWidth, m_renderHeight)) {
            setStatsText(QStringLiteral("Vulkan compute unavailable: %1. Falling back to CPU path tracer.")
                             .arg(m_vulkanTracer->lastError()));
            m_gpuModeActive.store(false, std::memory_order_relaxed);
            m_activeComputeKernel.clear();
        }
    }

    if (m_gpuModeActive.load(std::memory_order_relaxed)) {
        m_repaintRequests = 0;
        m_gpuUploadCalls.store(0, std::memory_order_relaxed);
        m_gpuUploadPixels.store(0, std::memory_order_relaxed);
        m_gpuUploadFrames.store(0, std::memory_order_relaxed);
        m_tileSize = chooseTileSize(api, m_renderWidth, m_renderHeight);
        m_maxUploadsPerFrame = chooseMaxUploadsPerFrame(api, m_renderWidth, m_renderHeight);
        m_renderTimer.restart();
        setProgress(0);
        setRendering(true);
        update();
        return;
    }

    {
        QMutexLocker lock(&m_mutex);
        m_image = QImage(m_renderWidth, m_renderHeight, QImage::Format_ARGB32);
        m_image.fill(Qt::black);
        m_pendingUploads.clear();
        m_fullUploadNeeded = true;
    }

    m_repaintRequests = 0;
    m_gpuUploadCalls.store(0, std::memory_order_relaxed);
    m_gpuUploadPixels.store(0, std::memory_order_relaxed);
    m_gpuUploadFrames.store(0, std::memory_order_relaxed);

    m_tileSize = chooseTileSize(api, m_renderWidth, m_renderHeight);
    m_maxUploadsPerFrame = chooseMaxUploadsPerFrame(api, m_renderWidth, m_renderHeight);

    m_renderTimer.restart();
    setProgress(0);
    setStatsText(QStringLiteral("Rendering..."));
    setRendering(true);
    update();

    m_thread = new QThread;
    m_worker = new RenderWorker(m_renderWidth, m_renderHeight, m_samples, m_maxDepth, m_tileSize);
    m_worker->moveToThread(m_thread);

    connect(m_thread, &QThread::started, m_worker, &RenderWorker::render);
    connect(m_worker, &RenderWorker::tileRendered, this, &RayTracerFboItem::onTileRendered, Qt::QueuedConnection);
    connect(m_worker, &RenderWorker::progressUpdated, this, &RayTracerFboItem::onWorkerProgressUpdated, Qt::QueuedConnection);
    connect(m_worker, &RenderWorker::finished, this, &RayTracerFboItem::onWorkerFinished, Qt::QueuedConnection);
    connect(m_worker, &RenderWorker::finished, m_thread, &QThread::quit);
    connect(m_thread, &QThread::finished, m_worker, &RenderWorker::deleteLater);
    connect(m_thread, &QThread::finished, m_thread, &QThread::deleteLater);

    m_thread->start();
}

void RayTracerFboItem::stopRender() {
    m_gpuModeActive.store(false, std::memory_order_relaxed);

    if (!m_rendering || !m_worker || !m_thread) {
        setRendering(false);
        return;
    }

    m_worker->stop();
    m_thread->quit();
    m_thread->wait();
    m_worker = nullptr;
    m_thread = nullptr;
    setRendering(false);
}

void RayTracerFboItem::onTileRendered(
    int yStart,
    int xStart,
    int tileWidth,
    int tileHeight,
    const QVector<unsigned int> &pixelData) {
    if (yStart < 0 || yStart >= m_image.height()) {
        return;
    }
    if (xStart < 0 || xStart >= m_image.width()) {
        return;
    }
    if (tileWidth <= 0 || tileHeight <= 0) {
        return;
    }
    if (xStart + tileWidth > m_image.width() || yStart + tileHeight > m_image.height()) {
        return;
    }
    if (pixelData.size() != tileWidth * tileHeight) {
        return;
    }

    {
        QMutexLocker lock(&m_mutex);
        for (int row = 0; row < tileHeight; ++row) {
            unsigned int *dest = reinterpret_cast<unsigned int *>(m_image.scanLine(yStart + row)) + xStart;
            const unsigned int *src = pixelData.constData() + row * tileWidth;
            std::memcpy(dest, src, static_cast<size_t>(tileWidth) * sizeof(unsigned int));
        }

        DirtyUpload upload;
        upload.yStart = yStart;
        upload.xStart = xStart;
        upload.width = tileWidth;
        upload.height = tileHeight;
        upload.pixels = pixelData;
        m_pendingUploads.push_back(std::move(upload));
    }

    ++m_repaintRequests;
    update();
}

void RayTracerFboItem::onWorkerProgressUpdated(int value) {
    setProgress(value);
}

void RayTracerFboItem::onWorkerFinished() {
    const qint64 elapsedMs = std::max<qint64>(1, m_renderTimer.elapsed());
    const double elapsedSec = static_cast<double>(elapsedMs) / 1000.0;
    const double totalSamples =
        static_cast<double>(m_renderWidth) *
        static_cast<double>(m_renderHeight) *
        static_cast<double>(m_samples);
    const double samplesPerSec = totalSamples / elapsedSec;
    const double refreshFps = static_cast<double>(m_repaintRequests) / elapsedSec;
    const double uploadCalls = static_cast<double>(m_gpuUploadCalls.load(std::memory_order_relaxed));
    const double uploadFrames = std::max<double>(1.0, static_cast<double>(m_gpuUploadFrames.load(std::memory_order_relaxed)));
    const double uploadPixels = static_cast<double>(m_gpuUploadPixels.load(std::memory_order_relaxed));
    const double uploadsPerFrame = uploadCalls / uploadFrames;
    const double uploadPixelsPerSec = uploadPixels / elapsedSec;

    setStatsText(QStringLiteral(
                     "Render %1s | Repaints %2 (%3 FPS) | Throughput %4 Msamples/s | GPU uploads %5/frame | Upload BW %6 MPix/s | Tile %7 | Max uploads/frame %8")
                     .arg(elapsedSec, 0, 'f', 2)
                     .arg(m_repaintRequests)
                     .arg(refreshFps, 0, 'f', 1)
                     .arg(samplesPerSec / 1e6, 0, 'f', 2)
                     .arg(uploadsPerFrame, 0, 'f', 2)
                     .arg(uploadPixelsPerSec / 1e6, 0, 'f', 2)
                     .arg(m_tileSize)
                     .arg(m_maxUploadsPerFrame));

    setProgress(100);
    setRendering(false);
    m_worker = nullptr;
    m_thread = nullptr;
}

QSGNode *RayTracerFboItem::updatePaintNode(QSGNode *oldNode, QQuickItem::UpdatePaintNodeData *) {
    if (!window() || !window()->rendererInterface()) {
        return oldNode;
    }

    auto *node = static_cast<RayTracerTextureNode *>(oldNode);
    if (!node) {
        node = new RayTracerTextureNode;
    }

    const auto api = window()->rendererInterface()->graphicsApi();
    const bool isOpenGl = api == QSGRendererInterface::OpenGL;

    if (m_gpuModeActive.load(std::memory_order_relaxed)) {
        if (m_activeComputeKernel == QStringLiteral("opengl") && (!isOpenGl || !m_gpuTracer)) {
            m_gpuModeActive.store(false, std::memory_order_relaxed);
        } else if (m_activeComputeKernel == QStringLiteral("opengl")) {
            if (!m_gpuTracer->isReady() && !m_gpuTracer->initialize()) {
                const QString err = m_gpuTracer->lastError();
                QMetaObject::invokeMethod(this, [this, err]() {
                    setStatsText(QStringLiteral("OpenGL compute unavailable: %1. Falling back to CPU renderer.").arg(err));
                    m_gpuModeActive.store(false, std::memory_order_relaxed);
                    setRendering(false);
                }, Qt::QueuedConnection);
            } else {
                m_gpuTracer->resize(m_renderWidth, m_renderHeight);
                QSGTexture *texture = node->texture();
                const bool needNativeTexture = !texture || texture->textureSize() != QSize(m_renderWidth, m_renderHeight);
                if (needNativeTexture) {
                    texture = QNativeInterface::QSGOpenGLTexture::fromNative(
                        m_gpuTracer->outputTextureId(),
                        window(),
                        QSize(m_renderWidth, m_renderHeight),
                        {});
                    node->setTexture(texture);
                    node->setFiltering(QSGTexture::Linear);
                }

                if (m_rendering) {
                    m_gpuTracer->renderFrame(1, m_maxDepth);
                    const int frame = m_gpuTracer->frameIndex();
                    const int target = std::max(1, m_samples);
                    const int progress = std::min(100, (frame * 100) / target);
                    QMetaObject::invokeMethod(this, [this, progress]() { setProgress(progress); }, Qt::QueuedConnection);

                    if (frame >= target) {
                        const qint64 elapsedMs = std::max<qint64>(1, m_renderTimer.elapsed());
                        const double elapsedSec = static_cast<double>(elapsedMs) / 1000.0;
                        const double totalSamples =
                            static_cast<double>(m_renderWidth) *
                            static_cast<double>(m_renderHeight) *
                            static_cast<double>(target);
                        const double samplesPerSec = totalSamples / elapsedSec;

                        QMetaObject::invokeMethod(this, [this, elapsedSec, samplesPerSec]() {
                            setStatsText(QStringLiteral(
                                             "GPU Render %1s | Throughput %2 Msamples/s | Backend OpenGL compute")
                                             .arg(elapsedSec, 0, 'f', 2)
                                             .arg(samplesPerSec / 1e6, 0, 'f', 2));
                            setProgress(100);
                            setRendering(false);
                        }, Qt::QueuedConnection);
                    } else {
                        QMetaObject::invokeMethod(this, [this]() { update(); }, Qt::QueuedConnection);
                    }
                }

                const qreal iw = m_renderWidth;
                const qreal ih = m_renderHeight;
                const qreal w = width();
                const qreal h = height();
                const qreal imgAspect = iw / std::max<qreal>(1.0, ih);
                const qreal viewAspect = w / std::max<qreal>(1.0, h);

                QRectF targetRect;
                if (viewAspect > imgAspect) {
                    const qreal drawW = h * imgAspect;
                    targetRect = QRectF((w - drawW) * 0.5, 0.0, drawW, h);
                } else {
                    const qreal drawH = w / imgAspect;
                    targetRect = QRectF(0.0, (h - drawH) * 0.5, w, drawH);
                }

                node->setRect(targetRect);
                node->markDirty(QSGNode::DirtyGeometry | QSGNode::DirtyMaterial);
                return node;
            }
        } else if (m_activeComputeKernel == QStringLiteral("cuda")) {
            if (!m_cudaTracer) {
                m_gpuModeActive.store(false, std::memory_order_relaxed);
            } else {
                const bool needInit = m_cudaTracer->frameIndex() == 0;
                if (needInit && !m_cudaTracer->initialize(m_renderWidth, m_renderHeight)) {
                    const QString err = m_cudaTracer->lastError();
                    QMetaObject::invokeMethod(this, [this, err]() {
                        setStatsText(QStringLiteral("CUDA backend unavailable: %1. Falling back to CPU renderer.").arg(err));
                        m_gpuModeActive.store(false, std::memory_order_relaxed);
                        setRendering(false);
                    }, Qt::QueuedConnection);
                } else if (m_rendering) {
                    if (!m_cudaTracer->renderFrame(m_maxDepth)) {
                        const QString err = m_cudaTracer->lastError();
                        QMetaObject::invokeMethod(this, [this, err]() {
                            setStatsText(QStringLiteral("CUDA render failed: %1").arg(err));
                            setRendering(false);
                        }, Qt::QueuedConnection);
                    } else {
                        const unsigned int *pixels = m_cudaTracer->hostPixels();
                        if (pixels) {
                            QMutexLocker lock(&m_mutex);
                            for (int row = 0; row < m_renderHeight; ++row) {
                                unsigned int *dest = reinterpret_cast<unsigned int *>(m_image.scanLine(row));
                                const unsigned int *src = pixels + row * m_renderWidth;
                                std::memcpy(dest, src, static_cast<size_t>(m_renderWidth) * sizeof(unsigned int));
                            }
                            m_fullUploadNeeded = true;
                            m_pendingUploads.clear();
                        }

                        const int frame = m_cudaTracer->frameIndex();
                        const int target = std::max(1, m_samples);
                        const int progress = std::min(100, (frame * 100) / target);
                        QMetaObject::invokeMethod(this, [this, progress]() { setProgress(progress); }, Qt::QueuedConnection);

                        if (frame >= target) {
                            const qint64 elapsedMs = std::max<qint64>(1, m_renderTimer.elapsed());
                            const double elapsedSec = static_cast<double>(elapsedMs) / 1000.0;
                            const double totalSamples =
                                static_cast<double>(m_renderWidth) *
                                static_cast<double>(m_renderHeight) *
                                static_cast<double>(target);
                            const double samplesPerSec = totalSamples / elapsedSec;

                            QMetaObject::invokeMethod(this, [this, elapsedSec, samplesPerSec]() {
                                setStatsText(QStringLiteral(
                                                 "GPU Render %1s | Throughput %2 Msamples/s | Backend CUDA")
                                                 .arg(elapsedSec, 0, 'f', 2)
                                                 .arg(samplesPerSec / 1e6, 0, 'f', 2));
                                setProgress(100);
                                setRendering(false);
                            }, Qt::QueuedConnection);
                        } else {
                            QMetaObject::invokeMethod(this, [this]() { update(); }, Qt::QueuedConnection);
                        }
                    }
                }
            }
        } else if (m_activeComputeKernel == QStringLiteral("vulkan")) {
            if (!m_vulkanTracer) {
                m_gpuModeActive.store(false, std::memory_order_relaxed);
            } else if (m_rendering) {
                if (!m_vulkanTracer->renderFrame(m_maxDepth)) {
                    const QString err = m_vulkanTracer->lastError();
                    QMetaObject::invokeMethod(this, [this, err]() {
                        setStatsText(QStringLiteral("Vulkan compute render failed: %1").arg(err));
                        setRendering(false);
                    }, Qt::QueuedConnection);
                } else {
                    const unsigned int *pixels = m_vulkanTracer->hostPixels();
                    if (pixels) {
                        QMutexLocker lock(&m_mutex);
                        for (int row = 0; row < m_renderHeight; ++row) {
                            unsigned int *dest = reinterpret_cast<unsigned int *>(m_image.scanLine(row));
                            const unsigned int *src = pixels + row * m_renderWidth;
                            std::memcpy(dest, src, static_cast<size_t>(m_renderWidth) * sizeof(unsigned int));
                        }
                        m_fullUploadNeeded = true;
                        m_pendingUploads.clear();
                    }

                    const int frame = m_vulkanTracer->frameIndex();
                    const int target = std::max(1, m_samples);
                    const int progress = std::min(100, (frame * 100) / target);
                    QMetaObject::invokeMethod(this, [this, progress]() { setProgress(progress); }, Qt::QueuedConnection);

                    if (frame >= target) {
                        const qint64 elapsedMs = std::max<qint64>(1, m_renderTimer.elapsed());
                        const double elapsedSec = static_cast<double>(elapsedMs) / 1000.0;
                        const double totalSamples =
                            static_cast<double>(m_renderWidth) *
                            static_cast<double>(m_renderHeight) *
                            static_cast<double>(target);
                        const double samplesPerSec = totalSamples / elapsedSec;

                        QMetaObject::invokeMethod(this, [this, elapsedSec, samplesPerSec]() {
                            setStatsText(QStringLiteral(
                                             "GPU Render %1s | Throughput %2 Msamples/s | Backend Vulkan compute")
                                             .arg(elapsedSec, 0, 'f', 2)
                                             .arg(samplesPerSec / 1e6, 0, 'f', 2));
                            setProgress(100);
                            setRendering(false);
                        }, Qt::QueuedConnection);
                    } else {
                        QMetaObject::invokeMethod(this, [this]() { update(); }, Qt::QueuedConnection);
                    }
                }
            }
        }
    }

    QImage imageCopy;
    QVector<DirtyUpload> uploads;
    bool fullUpload = false;
    bool hasRemainingUploads = false;

    {
        QMutexLocker lock(&m_mutex);
        imageCopy = m_image;

        if (m_fullUploadNeeded) {
            fullUpload = true;
            m_fullUploadNeeded = false;
            m_pendingUploads.clear();
        } else if (!m_pendingUploads.isEmpty()) {
            const int uploadCount = std::min(m_maxUploadsPerFrame, static_cast<int>(m_pendingUploads.size()));
            uploads.reserve(uploadCount);
            for (int i = 0; i < uploadCount; ++i) {
                uploads.push_back(std::move(m_pendingUploads[i]));
            }
            if (uploadCount < m_pendingUploads.size()) {
                QVector<DirtyUpload> remaining;
                remaining.reserve(m_pendingUploads.size() - uploadCount);
                for (int i = uploadCount; i < m_pendingUploads.size(); ++i) {
                    remaining.push_back(std::move(m_pendingUploads[i]));
                }
                m_pendingUploads = std::move(remaining);
                hasRemainingUploads = true;
            } else {
                m_pendingUploads.clear();
            }
        }
    }

    if (hasRemainingUploads) {
        QMetaObject::invokeMethod(this, [this]() { update(); }, Qt::QueuedConnection);
    }

    QSGTexture *texture = node->texture();
    const bool needNewTexture = !texture || texture->textureSize() != imageCopy.size();
    if (needNewTexture) {
        texture = window()->createTextureFromImage(imageCopy, {});
        node->setTexture(texture);
        node->setFiltering(QSGTexture::Linear);
        fullUpload = false;
        uploads.clear();
    } else {
        node->setFiltering(QSGTexture::Linear);
    }

    quint64 frameUploadCalls = 0;
    quint64 frameUploadPixels = 0;

#if QT_CONFIG(opengl)
    if (isOpenGl && texture) {
        auto *native = texture->nativeInterface<QNativeInterface::QSGOpenGLTexture>();
        QOpenGLContext *ctx = QOpenGLContext::currentContext();
        if (native && ctx) {
            QOpenGLFunctions *gl = ctx->functions();
            gl->glBindTexture(GL_TEXTURE_2D, native->nativeTexture());
            gl->glPixelStorei(GL_UNPACK_ALIGNMENT, 4);

            if (fullUpload) {
                gl->glTexSubImage2D(
                    GL_TEXTURE_2D,
                    0,
                    0,
                    0,
                    imageCopy.width(),
                    imageCopy.height(),
                    GL_BGRA,
                    GL_UNSIGNED_BYTE,
                    imageCopy.constBits());
                frameUploadCalls += 1;
                frameUploadPixels += static_cast<quint64>(imageCopy.width()) * static_cast<quint64>(imageCopy.height());
            } else {
                for (const DirtyUpload &upload : uploads) {
                    if (upload.pixels.isEmpty()) {
                        continue;
                    }
                    gl->glTexSubImage2D(
                        GL_TEXTURE_2D,
                        0,
                        upload.xStart,
                        upload.yStart,
                        upload.width,
                        upload.height,
                        GL_BGRA,
                        GL_UNSIGNED_BYTE,
                        upload.pixels.constData());
                    frameUploadCalls += 1;
                    frameUploadPixels += static_cast<quint64>(upload.width) * static_cast<quint64>(upload.height);
                }
            }
            gl->glBindTexture(GL_TEXTURE_2D, 0);
        } else if (fullUpload || !uploads.isEmpty()) {
            texture = window()->createTextureFromImage(imageCopy, {});
            node->setTexture(texture);
            frameUploadCalls += 1;
            frameUploadPixels += static_cast<quint64>(imageCopy.width()) * static_cast<quint64>(imageCopy.height());
        }
    } else if (texture && (fullUpload || !uploads.isEmpty())) {
        texture = window()->createTextureFromImage(imageCopy, {});
        node->setTexture(texture);
        frameUploadCalls += 1;
        frameUploadPixels += static_cast<quint64>(imageCopy.width()) * static_cast<quint64>(imageCopy.height());
    }
#endif

    if (frameUploadCalls > 0) {
        m_gpuUploadCalls.fetch_add(frameUploadCalls, std::memory_order_relaxed);
        m_gpuUploadPixels.fetch_add(frameUploadPixels, std::memory_order_relaxed);
        m_gpuUploadFrames.fetch_add(1, std::memory_order_relaxed);
    }

    const qreal iw = imageCopy.width();
    const qreal ih = imageCopy.height();
    const qreal w = width();
    const qreal h = height();
    const qreal imgAspect = iw / std::max<qreal>(1.0, ih);
    const qreal viewAspect = w / std::max<qreal>(1.0, h);

    QRectF targetRect;
    if (viewAspect > imgAspect) {
        const qreal drawW = h * imgAspect;
        targetRect = QRectF((w - drawW) * 0.5, 0.0, drawW, h);
    } else {
        const qreal drawH = w / imgAspect;
        targetRect = QRectF(0.0, (h - drawH) * 0.5, w, drawH);
    }

    node->setRect(targetRect);
    node->markDirty(QSGNode::DirtyGeometry | QSGNode::DirtyMaterial);
    return node;
}

void RayTracerFboItem::releaseResources() {
    QQuickItem::releaseResources();
}

void RayTracerFboItem::setRendering(bool value) {
    if (m_rendering == value) {
        return;
    }
    m_rendering = value;
    emit renderingChanged();
}

void RayTracerFboItem::setProgress(int value) {
    value = std::clamp(value, 0, 100);
    if (m_progress == value) {
        return;
    }
    m_progress = value;
    emit progressChanged();
}

void RayTracerFboItem::setStatsText(const QString &value) {
    if (m_statsText == value) {
        return;
    }
    m_statsText = value;
    emit statsTextChanged();
}

int RayTracerFboItem::chooseTileSize(QSGRendererInterface::GraphicsApi api, int width, int height) const {
    const int pixels = width * height;
    int tileSize = 16;

    if (pixels >= 1920 * 1080) {
        tileSize = 24;
    }
    if (pixels >= 2560 * 1440) {
        tileSize = 32;
    }

    switch (api) {
    case QSGRendererInterface::Vulkan:
    case QSGRendererInterface::Direct3D11:
    case QSGRendererInterface::Metal:
        tileSize += 8;
        break;
    case QSGRendererInterface::OpenGL:
        break;
    case QSGRendererInterface::Software:
        tileSize = 16;
        break;
    default:
        break;
    }

    return std::clamp(tileSize, 8, 48);
}

int RayTracerFboItem::chooseMaxUploadsPerFrame(QSGRendererInterface::GraphicsApi api, int width, int height) const {
    const int pixels = width * height;
    int maxUploads = 24;

    if (pixels >= 1920 * 1080) {
        maxUploads = 20;
    }
    if (pixels >= 2560 * 1440) {
        maxUploads = 16;
    }

    switch (api) {
    case QSGRendererInterface::Vulkan:
    case QSGRendererInterface::Direct3D11:
    case QSGRendererInterface::Metal:
        maxUploads += 6;
        break;
    case QSGRendererInterface::OpenGL:
        break;
    case QSGRendererInterface::Software:
        maxUploads = 8;
        break;
    default:
        break;
    }

    return std::clamp(maxUploads, 8, 40);
}
