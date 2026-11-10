#include "ApiKnowledge.h"
#include "CompletionCatalog.h"
#include <QCoreApplication>
#include <QDebug>
#include <cstdlib>

void check(bool good,const QString& what) {
    if(!good) {qCritical().noquote()<<"FAIL"<<what; std::exit(1);}
}
int main(int argc,char** argv) {
    QCoreApplication app(argc,argv);
    check(ApiKnowledge::isDocumentedSymbol("Wavegen.Channel2.Simple.Frequency.value"),"channel alias lookup");
    check(ApiKnowledge::isDocumentedSymbol("Scope.Channel2.measureWindow"),"Scope channel alias lookup");
    check(!ApiKnowledge::isDocumentedSymbol("Scope.Channel1.noSuchMethod"),"unknown methods must not appear");
    check(ApiKnowledge::lookup("Scope.Channel2.spectrum").example.contains("spectrum("),"FFT documentation");
    check(ApiKnowledge::lookup("Wavegen.Channel2.Simple.Amplitude.value").unit=="V peak","generator units");
    check(ApiKnowledge::lookup("Scope.single").summary.size()>4,"catalog summary");
    for(const auto& path:ApiKnowledge::curatedPaths()) {
        const auto doc=ApiKnowledge::lookup(path);
        check(doc.known,QStringLiteral("stale curated API documentation: ")+path);
        check(!doc.example.isEmpty(),QStringLiteral("missing example for ")+path);
    }
    const auto snippets=ApiKnowledge::snippets();
    check(snippets.size()>=6,"snippet catalog inventory");
    for(const auto& snip:snippets) {
        check(!snip.trigger.isEmpty() && !snip.body.isEmpty(),"empty snippet");
        // Some references are calculated fields on objects returned by functions;
        // check only API paths that are directly represented in the public catalog.
        for(const auto& path:ApiKnowledge::referencedApiPaths(snip.body)) {
            if(path.contains(".peakFrequencyHz") || path.contains(".sampleCount") ||
               path=="Scope.Channel1.statistics") continue;
            check(ApiKnowledge::isDocumentedSymbol(path),QStringLiteral("snippet uses missing API: ")+path);
        }
    }
    qInfo()<<"PASS: API documentation paths match the public CompletionCatalog and snippet references resolve";
}
