#include "MainWindow.h"

#include <QApplication>
#include <QIcon>
#include <QStyleFactory>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("WFScript"));
    app.setApplicationVersion(QStringLiteral("0.1.0"));
    app.setOrganizationName(QStringLiteral("WFScript"));
    app.setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
    app.setWindowIcon(QIcon(QStringLiteral(":/wfscript/icons/app_icon.png")));

    MainWindow window;
    window.show();
    return app.exec();
}
