#include "RenderWidget.h"
#include <QDebug>
#include <QEventLoop>
#include <algorithm>
#include <atomic>
#include <thread>
#include <cstring>

// --- RenderWorker Implementation ---

RenderWorker::RenderWorker(int width, int height, int samples, int depth, QObject* parent)
    : QObject(parent), m_width(width), m_height(height), m_samples(samples), m_depth(depth)
{
}

void RenderWorker::stop() {
    m_stop.store(true, std::memory_order_relaxed);
}

void RenderWorker::render() {
    m_stop.store(false, std::memory_order_relaxed);

    // Setup Scene
    auto aspect_ratio = double(m_width) / m_height;
    Point3 lookfrom(13,2,3);
    Point3 lookat(0,0,0);
    Vec3 vup(0,1,0); 
    auto dist_to_focus = 10.0;
    auto aperture = 0.1;

    Camera cam(lookfrom, lookat, vup, 20, aspect_ratio, aperture, dist_to_focus);
    HitableList world_list = random_scene();
    std::vector<std::shared_ptr<Hitable>> world_objects = world_list.objects;
    BVHNode world(world_objects, 0, world_objects.size());

    const int widthDenom = std::max(1, m_width - 1);
    const int heightDenom = std::max(1, m_height - 1);
    const double invWidthDenom = 1.0 / static_cast<double>(widthDenom);
    const double invHeightDenom = 1.0 / static_cast<double>(heightDenom);
    const double scale = 1.0 / static_cast<double>(m_samples);

    constexpr int tileSize = 16;
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

                for (int line = yStart; line < yEnd; ++line) {
                    const int j = m_height - 1 - line;
                    QVector<unsigned int> tileRowData(xEnd - xStart);

                    for (int i = xStart; i < xEnd; ++i) {
                        Color pixel_color(0,0,0);
                        for (int s = 0; s < m_samples; ++s) {
                            const double u = (static_cast<double>(i) + random_double()) * invWidthDenom;
                            const double v = (static_cast<double>(j) + random_double()) * invHeightDenom;
                            Ray r = cam.get_ray(u, v);
                            pixel_color += ray_color(r, world, m_depth);
                        }

                        const double r = std::sqrt(scale * pixel_color.x());
                        const double g = std::sqrt(scale * pixel_color.y());
                        const double b = std::sqrt(scale * pixel_color.z());

                        const int ir = static_cast<int>(256 * clamp(r, 0.0, 0.999));
                        const int ig = static_cast<int>(256 * clamp(g, 0.0, 0.999));
                        const int ib = static_cast<int>(256 * clamp(b, 0.0, 0.999));

                        tileRowData[i - xStart] = (255u << 24) | (static_cast<unsigned int>(ir) << 16) |
                                                  (static_cast<unsigned int>(ig) << 8) | static_cast<unsigned int>(ib);
                    }

                    emit tileRendered(line, xStart, tileRowData);
                }

                const int done = completedTiles.fetch_add(1, std::memory_order_relaxed) + 1;
                emit progressUpdated(static_cast<int>((100.0 * done) / totalTiles));
            }
        });
    }

    for (std::thread& worker : workers) {
        worker.join();
    }

    emit finished();
}

// --- RenderWidget Implementation ---

RenderWidget::RenderWidget(QWidget *parent) : QOpenGLWidget(parent)
{
    setMinimumSize(400, 225);
    m_image = QImage(m_width, m_height, QImage::Format_ARGB32);
    m_image.fill(Qt::black);
}

RenderWidget::~RenderWidget() {
    stopRender();
    if (context()) {
        makeCurrent();
        destroyGlResources();
        doneCurrent();
    }
}

void RenderWidget::startRender() {
    if (m_isRendering) return;

    m_image = QImage(m_width, m_height, QImage::Format_ARGB32);
    m_image.fill(Qt::black);
    m_textureDirty = true;
    m_renderTimer.restart();
    m_updateThrottleTimer.restart();
    m_lastUpdateMs = 0;
    m_repaintRequests = 0;
    update();

    m_thread = new QThread;
    m_worker = new RenderWorker(m_width, m_height, m_samples, m_depth);
    m_worker->moveToThread(m_thread);

    connect(m_thread, &QThread::started, m_worker, &RenderWorker::render);
    connect(m_worker, &RenderWorker::tileRendered, this, &RenderWidget::updateTile);
    connect(m_worker, &RenderWorker::progressUpdated, this, &RenderWidget::progressChanged);
    connect(m_worker, &RenderWorker::finished, this, &RenderWidget::onWorkerFinished);
    connect(m_worker, &RenderWorker::finished, m_thread, &QThread::quit);
    connect(m_thread, &QThread::finished, m_thread, &QThread::deleteLater);
    connect(m_thread, &QThread::finished, m_worker, &RenderWorker::deleteLater);

    m_isRendering = true;
    m_thread->start();
}

void RenderWidget::stopRender() {
    if (m_isRendering && m_worker) {
        m_worker->stop();
        m_thread->quit();
        m_thread->wait();
        m_isRendering = false;
    }
}

bool RenderWidget::isRendering() const {
    return m_isRendering;
}

void RenderWidget::updateTile(int line, int xStart, const QVector<unsigned int>& pixelData) {
    if (line < 0 || line >= m_image.height()) return;

    if (xStart < 0 || xStart >= m_image.width()) return;
    if (pixelData.isEmpty()) return;
    if (xStart + pixelData.size() > m_image.width()) return;

    unsigned int* dest = reinterpret_cast<unsigned int*>(m_image.scanLine(line)) + xStart;
    std::memcpy(dest, pixelData.data(), static_cast<size_t>(pixelData.size()) * sizeof(unsigned int));
    m_textureDirty = true;

    const qint64 elapsedMs = m_updateThrottleTimer.elapsed();
    if (elapsedMs - m_lastUpdateMs >= 16) {
        m_lastUpdateMs = elapsedMs;
        ++m_repaintRequests;
        update();
    }
}

void RenderWidget::onWorkerFinished() {
    ++m_repaintRequests;
    update();

    const qint64 elapsedMs = std::max<qint64>(1, m_renderTimer.elapsed());
    const double elapsedSec = static_cast<double>(elapsedMs) / 1000.0;
    const double totalSamples = static_cast<double>(m_width) * static_cast<double>(m_height) * static_cast<double>(m_samples);
    const double samplesPerSec = totalSamples / elapsedSec;
    const double refreshFps = static_cast<double>(m_repaintRequests) / elapsedSec;

    const QString statsText = QString("Render %1s | Repaints %2 (%3 FPS) | Throughput %4 Msamples/s")
        .arg(elapsedSec, 0, 'f', 2)
        .arg(m_repaintRequests)
        .arg(refreshFps, 0, 'f', 1)
        .arg(samplesPerSec / 1e6, 0, 'f', 2);
    emit renderStatsUpdated(statsText);

    m_isRendering = false;
    m_thread = nullptr;
    m_worker = nullptr;
    emit renderFinished();
}

void RenderWidget::initializeGL() {
    initializeOpenGLFunctions();

    static const char* vertexShaderSrc = R"(
        attribute vec2 aPos;
        attribute vec2 aUv;
        varying vec2 vUv;
        void main() {
            vUv = aUv;
            gl_Position = vec4(aPos, 0.0, 1.0);
        }
    )";

    static const char* fragmentShaderSrc = R"(
        varying vec2 vUv;
        uniform sampler2D uTex;
        void main() {
            gl_FragColor = texture2D(uTex, vUv);
        }
    )";

    m_program = std::make_unique<QOpenGLShaderProgram>();
    if (!m_program->addShaderFromSourceCode(QOpenGLShader::Vertex, vertexShaderSrc)) {
        qWarning() << "Failed to compile vertex shader:" << m_program->log();
    }
    if (!m_program->addShaderFromSourceCode(QOpenGLShader::Fragment, fragmentShaderSrc)) {
        qWarning() << "Failed to compile fragment shader:" << m_program->log();
    }
    if (!m_program->link()) {
        qWarning() << "Failed to link shader program:" << m_program->log();
    }

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
}

void RenderWidget::resizeGL(int, int) {
}

void RenderWidget::ensureTexture() {
    const bool textureSizeMismatch = m_texture &&
        (m_texture->width() != m_image.width() || m_texture->height() != m_image.height());

    if (!m_texture || textureSizeMismatch) {
        m_texture.reset();
        m_texture = std::make_unique<QOpenGLTexture>(QOpenGLTexture::Target2D);
        m_texture->create();
        m_texture->setSize(m_image.width(), m_image.height());
        m_texture->setFormat(QOpenGLTexture::RGBA8_UNorm);
        m_texture->allocateStorage(QOpenGLTexture::BGRA, QOpenGLTexture::UInt8);
        m_texture->setWrapMode(QOpenGLTexture::ClampToEdge);
        m_texture->setMinificationFilter(QOpenGLTexture::Linear);
        m_texture->setMagnificationFilter(QOpenGLTexture::Linear);
        m_textureDirty = true;
    }

    if (m_textureDirty) {
        m_texture->setData(QOpenGLTexture::BGRA, QOpenGLTexture::UInt8, m_image.constBits());
        m_textureDirty = false;
    }
}

void RenderWidget::paintGL() {
    glClear(GL_COLOR_BUFFER_BIT);

    if (!m_program) {
        return;
    }

    ensureTexture();
    if (!m_texture) {
        return;
    }

    const float imageAspect = static_cast<float>(m_image.width()) / static_cast<float>(std::max(1, m_image.height()));
    const float viewAspect = static_cast<float>(std::max(1, width())) / static_cast<float>(std::max(1, height()));

    int viewportW = width();
    int viewportH = height();
    int viewportX = 0;
    int viewportY = 0;

    if (viewAspect > imageAspect) {
        viewportW = static_cast<int>(height() * imageAspect);
        viewportX = (width() - viewportW) / 2;
    } else {
        viewportH = static_cast<int>(width() / imageAspect);
        viewportY = (height() - viewportH) / 2;
    }

    glViewport(viewportX, viewportY, viewportW, viewportH);

    static const GLfloat quadVertices[] = {
        -1.0f, -1.0f, 0.0f, 1.0f,
         1.0f, -1.0f, 1.0f, 1.0f,
        -1.0f,  1.0f, 0.0f, 0.0f,
         1.0f,  1.0f, 1.0f, 0.0f
    };

    m_program->bind();
    m_texture->bind(0);
    m_program->setUniformValue("uTex", 0);

    const int posLocation = m_program->attributeLocation("aPos");
    const int uvLocation = m_program->attributeLocation("aUv");
    if (posLocation >= 0) {
        m_program->enableAttributeArray(posLocation);
        m_program->setAttributeArray(posLocation, GL_FLOAT, quadVertices, 2, 4 * static_cast<int>(sizeof(GLfloat)));
    }
    if (uvLocation >= 0) {
        m_program->enableAttributeArray(uvLocation);
        m_program->setAttributeArray(uvLocation, GL_FLOAT, quadVertices + 2, 2, 4 * static_cast<int>(sizeof(GLfloat)));
    }

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    if (posLocation >= 0) {
        m_program->disableAttributeArray(posLocation);
    }
    if (uvLocation >= 0) {
        m_program->disableAttributeArray(uvLocation);
    }
    m_texture->release();
    m_program->release();

    glViewport(0, 0, width(), height());
}

void RenderWidget::destroyGlResources() {
    m_texture.reset();
    m_program.reset();
}
