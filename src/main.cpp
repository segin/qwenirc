#include "frontend/MainWindow.h"
#include <QApplication>
#include <QSettings>
#include <QStyleFactory>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);

    app.setApplicationName("QwenIRC");
    app.setApplicationVersion("0.1.0");
    app.setOrganizationName("QwenIRC");

    QApplication::setStyle(QStyleFactory::create("Fusion"));

    MainWindow window;
    window.show();

    return app.exec();
}
