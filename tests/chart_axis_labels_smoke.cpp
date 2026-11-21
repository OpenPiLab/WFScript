#include "plot/PlotAxisLabels.h"
#include <QCoreApplication>
#include <QDebug>
#include <cmath>
#define CHECK(e) do{ if(!(e)){ qCritical()<<"Axis label failure:"<<#e<<__LINE__;return 1; } }while(false)
static bool unique(const QVector<QString>& strings) {
    for(int i=1;i<strings.size();++i)if(strings[i]==strings[i-1])return false;
    return true;
}
int main(int argc,char** argv){
    QCoreApplication app(argc,argv);
    const auto close=PlotAxisLabels::linearX({0.024990,0.024995,0.025000,0.025005,0.025010});
    CHECK(close.size()==5);
    CHECK(unique(close));
    CHECK(close[0]==QStringLiteral("24.990m"));
    CHECK(close[2]==QStringLiteral("25.000m"));
    CHECK(close[4]==QStringLiteral("25.010m"));
    const auto nearGlitch=PlotAxisLabels::linearX({0.001495,0.001500,0.001505,0.001510});
    CHECK(unique(nearGlitch));
    const auto signedTicks=PlotAxisLabels::linearX({-0.000020,-0.000010,0.0,0.000010,0.000020});
    CHECK(unique(signedTicks));
    CHECK(signedTicks[2].startsWith(QStringLiteral("0")));
    const auto ordinary=PlotAxisLabels::linearX({0.0,0.25,0.5,0.75,1.0});
    CHECK(unique(ordinary));
    const auto digital=PlotAxisLabels::linearX({0.125001,0.125002,0.125003});
    CHECK(unique(digital));
    qInfo()<<"PASS adaptive axis labels:"<<close<<nearGlitch;
    return 0;
}
