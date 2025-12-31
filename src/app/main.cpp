#include "Application.h"
#include "Translator.h"
#include "qt/MainWindow.h"

#include <QApplication>
#include <QMessageBox>
#include <QFont>

// COM headers must come before Windows.h with WIN32_LEAN_AND_MEAN
#include <combaseapi.h>
#include <Windows.h>

int main(int argc, char* argv[])
{
    // Initialize COM for WASAPI
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    if (FAILED(hr)) {
        return 1;
    }

    // Create Qt application
    QApplication qtApp(argc, argv);
    qtApp.setApplicationName("OnlyAir");
    qtApp.setOrganizationName("OnlyAir");
    qtApp.setApplicationVersion("1.0.0");

    // Set application-wide font
    QFont font = qtApp.font();
    font.setFamily("Segoe UI");
    qtApp.setFont(font);

    // Initialize translations based on system locale
    OnlyAir::Translator::instance().initialize();

    // Create core application
    OnlyAir::Application app;
    if (!app.initialize()) {
        QMessageBox::critical(nullptr, TR("error"), TR("error_init"));
        CoUninitialize();
        return 1;
    }

    // Create and show main window
    OnlyAir::MainWindow mainWindow(&app);
    mainWindow.show();

    // Run Qt event loop
    int result = qtApp.exec();

    // Cleanup
    app.shutdown();
    CoUninitialize();

    return result;
}
