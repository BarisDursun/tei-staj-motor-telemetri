#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include "enginemodel.h" // C++ tarafındaki arka plan (backend) motor mantığımız

int main(int argc, char *argv[]) {
    qputenv("QT_QUICK_BACKEND", "software");   // GPU/sürücü kaynaklı ekran titremesi çözer


    QGuiApplication app(argc, argv);    // Qt GUI app başlatır
    EngineModel engineModel;    //C++ Nesnesini Yarat
    QQmlApplicationEngine engine;   //QML ekler

    // (C++ İLE QML KÖPRÜSÜ):
    engine.rootContext()->setContextProperty("engineModel", &engineModel);  // C++'ta ürettiğimiz engineModeli, QML tarafına aynı isimle engineModel ismiyle bağlar


    engine.load(QUrl(QStringLiteral("qrc:/qml/Main.qml")));                 //UI yükler

    return app.exec();   //loop
}