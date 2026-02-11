#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QPushButton>
#include <QSpinBox>
#include <QProgressBar>
#include <QLabel>
#include "RenderWidget.h"

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onStartClicked();
    void onRenderFinished();
    void onProgressChanged(int value);
    void onRenderStatsUpdated(const QString& statsText);

private:
    RenderWidget *m_renderWidget;
    
    // Controls
    QSpinBox *m_spinWidth;
    QSpinBox *m_spinHeight;
    QSpinBox *m_spinSamples;
    QSpinBox *m_spinDepth;
    QPushButton *m_btnStart;
    QProgressBar *m_progressBar;
    QLabel *m_statsLabel;
    
    void setupUi();
};

#endif // MAINWINDOW_H
