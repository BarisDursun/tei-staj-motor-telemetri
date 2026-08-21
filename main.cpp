#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include "enginemodel.h"

int main(int argc, char *argv[]) {
    // "basic" render loop ve native OpenGL'e zorlama denendi, bu makinedeki
    // GPU/surucu kombinasyonunda beyaz flash/titreme sorununu cozmedi.
    // Kesin cozum: GPU render'i tamamen devre disi birakip Qt Quick'i
    // yazilimsal (raster) render'a zorlamak - donanim/surucu kaynakli hicbir
    // flicker olmaz. Bu kadar basit bir arayuz icin performans farki
    // hissedilmez. QGuiApplication olusmadan ONCE set edilmeli.
    qputenv("QT_QUICK_BACKEND", "software");

    QGuiApplication app(argc, argv);

    EngineModel engineModel;

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("engineModel", &engineModel);
    engine.load(QUrl(QStringLiteral("qrc:/qml/Main.qml")));

    return app.exec();
}
