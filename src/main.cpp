#include "frontend/MainWindow.h"
#include <QApplication>
#include <QCommandLineParser>
#include <QStyleFactory>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    QCoreApplication::setApplicationName("QwenIRC");
    QCoreApplication::setApplicationVersion("0.1.0");
    QCoreApplication::setOrganizationName("QwenIRC");
    QCoreApplication::setOrganizationDomain("qwenirc.qwen");
    
    QCommandLineParser parser;
    parser.setApplicationDescription("IRC client");
    parser.addHelpOption();
    parser.addVersionOption();
    
    QCommandLineOption hostOption(QStringList{"h", "host"}, "Server hostname", "host");
    parser.addOption(hostOption);
    
    QCommandLineOption portOption(QStringList{"p", "port"}, "Server port (default 6667)", "port");
    parser.addOption(portOption);
    
    QCommandLineOption tlsOption("tls", "Connect with TLS/SSL (port 6697)");
    parser.addOption(tlsOption);
    
    parser.addOption(QCommandLineOption("nick", "Nickname", "nick"));
    parser.addOption(QCommandLineOption("pass", "Server password", "pass"));
    
    parser.process(app);
    
    QApplication::setStyle(QStyleFactory::create("Fusion"));
    
    MainWindow window;
    window.show();

    return app.exec();
}
