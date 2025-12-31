#pragma once

#include <QMainWindow>
#include <QTimer>
#include <QLabel>
#include <QDragEnterEvent>
#include <QDropEvent>

namespace OnlyAir {

class Application;
class ScenePanel;
class PlaybackControls;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(Application* app, QWidget* parent = nullptr);
    ~MainWindow();

protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private slots:
    void onOpenFile();
    void onAbout();
    void updateUI();

private:
    void setupCentralWidget();
    void setupStatusBar();
    void setupShortcuts();
    void openFile(const QString& path);
    void applyDarkTheme();

    Application* m_app;
    ScenePanel* m_scenePanel;
    PlaybackControls* m_controls;

    // Status bar widgets
    QLabel* m_fileInfoLabel;
    QLabel* m_vcamStatus;
    QLabel* m_vaudioStatus;

    QTimer* m_updateTimer;
    QString m_currentFilePath;

    // Flag to skip SceneManager updates (for thumbnail-only updates)
    bool m_skipSceneManagerUpdate = false;
};

} // namespace OnlyAir
