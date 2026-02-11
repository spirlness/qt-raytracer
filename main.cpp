#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QGuiApplication>
#include <QProcess>
#include <QSurfaceFormat>
#include <QtCore/QObject>
#include <QtQml/QQmlContext>
#include <QtQuick/QQuickView>
#include <QtQuick/QQuickWindow>

#include "RayTracerFboItem.h"

static QSGRendererInterface::GraphicsApi parseGraphicsApi(const QString &name) {
    const QString normalized = name.trimmed().toLower();
    if (normalized == "opengl") {
        return QSGRendererInterface::OpenGL;
    }
    if (normalized == "vulkan") {
        return QSGRendererInterface::Vulkan;
    }
    if (normalized == "d3d11") {
        return QSGRendererInterface::Direct3D11;
    }
    if (normalized == "metal") {
        return QSGRendererInterface::Metal;
    }
    if (normalized == "software") {
        return QSGRendererInterface::Software;
    }
    return QSGRendererInterface::OpenGL;
}

static QString graphicsApiToString(QSGRendererInterface::GraphicsApi api) {
    switch (api) {
    case QSGRendererInterface::OpenGL:
        return QStringLiteral("opengl");
    case QSGRendererInterface::Vulkan:
        return QStringLiteral("vulkan");
    case QSGRendererInterface::Direct3D11:
        return QStringLiteral("d3d11");
    case QSGRendererInterface::Metal:
        return QStringLiteral("metal");
    case QSGRendererInterface::Software:
        return QStringLiteral("software");
    default:
        return QStringLiteral("opengl");
    }
}

class GraphicsBackendController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString currentBackend READ currentBackend CONSTANT)
    Q_PROPERTY(QString targetBackend READ targetBackend WRITE setTargetBackend NOTIFY targetBackendChanged)

public:
    explicit GraphicsBackendController(const QString &currentBackend, QObject *parent = nullptr)
        : QObject(parent), m_currentBackend(currentBackend), m_targetBackend(currentBackend) {
    }

    QString currentBackend() const {
        return m_currentBackend;
    }

    QString targetBackend() const {
        return m_targetBackend;
    }

    void setTargetBackend(const QString &value) {
        const QString normalized = value.trimmed().toLower();
        if (normalized.isEmpty() || m_targetBackend == normalized) {
            return;
        }
        m_targetBackend = normalized;
        emit targetBackendChanged();
    }

    Q_INVOKABLE bool applyAndRestart() {
        if (m_targetBackend == m_currentBackend) {
            return true;
        }

        const QString executable = QCoreApplication::applicationFilePath();
        const QStringList args{QStringLiteral("--graphics-api"), m_targetBackend};
        const bool ok = QProcess::startDetached(executable, args);
        if (ok) {
            QCoreApplication::quit();
        }
        return ok;
    }

signals:
    void targetBackendChanged();

private:
    QString m_currentBackend;
    QString m_targetBackend;
};

int main(int argc, char *argv[]) {
    QSurfaceFormat format = QSurfaceFormat::defaultFormat();
    format.setSamples(8);
    format.setSwapBehavior(QSurfaceFormat::DoubleBuffer);
    QSurfaceFormat::setDefaultFormat(format);

    QGuiApplication app(argc, argv);
    QCoreApplication::setApplicationName("Ray Tracing Simulator");

    QCommandLineParser parser;
    parser.setApplicationDescription("Qt Quick ray tracing simulator");
    parser.addHelpOption();

    QCommandLineOption graphicsApiOption(
        QStringList() << "g" << "graphics-api",
        "Preferred Qt Quick backend: opengl|vulkan|d3d11|metal|software",
        "graphics-api",
        "opengl");
    parser.addOption(graphicsApiOption);
    parser.process(app);

    const auto requestedApi = parseGraphicsApi(parser.value(graphicsApiOption));
    const QString requestedApiName = graphicsApiToString(requestedApi);

    QQuickWindow::setGraphicsApi(requestedApi);

    qmlRegisterType<RayTracerFboItem>("RayTracer", 1, 0, "RayTracerFboItem");

    QQuickView view;
    GraphicsBackendController backendController(requestedApiName);
    view.rootContext()->setContextProperty(QStringLiteral("backendController"), &backendController);
    view.setResizeMode(QQuickView::SizeRootObjectToView);
    view.setSource(QUrl(QStringLiteral("qrc:/qml/Main.qml")));
    if (view.status() == QQuickView::Error) {
        return -1;
    }
    view.setWidth(1200);
    view.setHeight(720);
    view.setTitle(QStringLiteral("Ray Tracing Simulator"));
    view.show();

    return app.exec();
}

#include "main.moc"
