#include "MainWindow.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setupUi();
    resize(1000, 600);
    setWindowTitle("Ray Tracing Simulator");
}

MainWindow::~MainWindow()
{
}

void MainWindow::setupUi() {
    QWidget *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    QHBoxLayout *mainLayout = new QHBoxLayout(centralWidget);

    // Left Side: Controls
    QVBoxLayout *controlsLayout = new QVBoxLayout();
    QGroupBox *settingsGroup = new QGroupBox("Settings", this);
    QFormLayout *formLayout = new QFormLayout(settingsGroup);

    m_spinWidth = new QSpinBox(this);
    m_spinWidth->setRange(100, 3840);
    m_spinWidth->setValue(400);

    m_spinHeight = new QSpinBox(this);
    m_spinHeight->setRange(100, 2160);
    m_spinHeight->setValue(225);

    m_spinSamples = new QSpinBox(this);
    m_spinSamples->setRange(1, 1000);
    m_spinSamples->setValue(10);

    m_spinDepth = new QSpinBox(this);
    m_spinDepth->setRange(1, 100);
    m_spinDepth->setValue(10);

    formLayout->addRow("Width:", m_spinWidth);
    formLayout->addRow("Height:", m_spinHeight);
    formLayout->addRow("Samples:", m_spinSamples);
    formLayout->addRow("Max Depth:", m_spinDepth);

    controlsLayout->addWidget(settingsGroup);

    m_btnStart = new QPushButton("Start Render", this);
    connect(m_btnStart, &QPushButton::clicked, this, &MainWindow::onStartClicked);
    controlsLayout->addWidget(m_btnStart);

    m_progressBar = new QProgressBar(this);
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    controlsLayout->addWidget(m_progressBar);

    m_statsLabel = new QLabel("Last render: N/A", this);
    m_statsLabel->setWordWrap(true);
    controlsLayout->addWidget(m_statsLabel);

    controlsLayout->addStretch();
    mainLayout->addLayout(controlsLayout, 1);

    // Right Side: Render View
    m_renderWidget = new RenderWidget(this);
    // Make sure render widget expands
    m_renderWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    
    // Connect render widget signals
    connect(m_renderWidget, &RenderWidget::renderFinished, this, &MainWindow::onRenderFinished);
    connect(m_renderWidget, &RenderWidget::progressChanged, this, &MainWindow::onProgressChanged);
    connect(m_renderWidget, &RenderWidget::renderStatsUpdated, this, &MainWindow::onRenderStatsUpdated);

    mainLayout->addWidget(m_renderWidget, 4);
}

void MainWindow::onStartClicked() {
    if (m_renderWidget->isRendering()) {
        m_renderWidget->stopRender();
        m_btnStart->setText("Start Render");
        // Re-enable controls
        m_spinWidth->setEnabled(true);
        m_spinHeight->setEnabled(true);
        m_spinSamples->setEnabled(true);
        m_spinDepth->setEnabled(true);
    } else {
        // Apply settings
        m_renderWidget->setResolution(m_spinWidth->value(), m_spinHeight->value());
        m_renderWidget->setSamples(m_spinSamples->value());
        m_renderWidget->setDepth(m_spinDepth->value());

        // Disable controls
        m_spinWidth->setEnabled(false);
        m_spinHeight->setEnabled(false);
        m_spinSamples->setEnabled(false);
        m_spinDepth->setEnabled(false);

        m_btnStart->setText("Stop Render");
        m_progressBar->setValue(0);
        m_statsLabel->setText("Rendering...");
        m_renderWidget->startRender();
    }
}

void MainWindow::onRenderFinished() {
    m_btnStart->setText("Start Render");
    m_spinWidth->setEnabled(true);
    m_spinHeight->setEnabled(true);
    m_spinSamples->setEnabled(true);
    m_spinDepth->setEnabled(true);
    m_progressBar->setValue(100);
}

void MainWindow::onProgressChanged(int value) {
    m_progressBar->setValue(value);
}

void MainWindow::onRenderStatsUpdated(const QString& statsText) {
    m_statsLabel->setText(statsText);
}
