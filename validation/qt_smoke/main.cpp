#include <QApplication>
#include <QLabel>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QDebug>
#include <QVariant>
#include "modules/analysis/AnalysisEngine.h"
#include "modules/analysis/MockServices.h"
#include <iostream>
int main(int argc,char** argv) {
    QApplication app(argc,argv);
    QLabel label("Qt 6.5.3 installation check");
    std::vector<motion::PoseResult> poses(5);
    const double angles[] = {170,140,90,140,170};
    for (int i=0;i<5;++i) {
        poses[i].timestampMs=i*200; poses[i].leftKnee.valid=true;
        poses[i].leftKnee.degrees=angles[i]; poses[i].sessionId="qt-check";
    }
    motion::AnalysisEngine engine;
    const auto result=engine.analyze(poses);
    if(!result.result.ok() || result.metrics.kneeRomDeg!=80 || result.metrics.cycles.size()!=1 ||
       result.metrics.qualityScoreAvailable) return 3;
    label.setText(QString("ROM: %1, cycles: %2").arg(result.metrics.kneeRomDeg).arg(result.metrics.cycles.size()));
    label.show(); app.processEvents(); label.close();
    auto db=QSqlDatabase::addDatabase("QSQLITE");
    db.setDatabaseName(":memory:");
    if(!db.open()) return 1;
    QSqlQuery q;
    if(!q.exec("CREATE TABLE test (id INTEGER)") || !q.exec("INSERT INTO test VALUES (1)") ||
       !q.exec("SELECT COUNT(*) FROM test") || !q.next() || q.value(0).toInt()!=1) return 2;
    qInfo() << "Qt version:" << qVersion() << "Widgets and SQLite OK";
    std::cout << "Qt " << qVersion() << ": Widgets + SQLite + real analysis integration OK\n";
    return 0;
}
