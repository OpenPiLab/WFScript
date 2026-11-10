#include "PowerSuppliesBridge.h"
#include "DwfRuntime.h"
#include <QtGlobal>
#include <cmath>

PowerSuppliesBridge::PowerSuppliesBridge(DwfRuntime* runtime, QObject* parent)
    :QObject(parent),m_runtime(runtime) {}

bool PowerSuppliesBridge::config() {
    m_error.clear();
    const auto& plus=m_outputs.positiveChannel();
    const auto& minus=m_outputs.negativeChannel();
    if ((plus.enabled()!=0 && plus.enabled()!=1) ||
        (minus.enabled()!=0 && minus.enabled()!=1) ||
        !std::isfinite(plus.volts()) || !std::isfinite(minus.volts()) ||
        plus.volts()<0.5 || plus.volts()>5.0 ||
        minus.volts()<-5.0 || minus.volts()>-0.5) {
        m_error=QStringLiteral("Invalid AD3 supply settings. Enables must be 0/1; V+ 0.5..5 V; V- -5..-0.5 V");
        // Invalid commands cannot be committed. If a prior configuration ever
        // touched the device, disable the master before reporting failure.
        bool stopConfirmed=true;
        if (m_touched && m_runtime) {
            QString stopError;
            stopConfirmed=m_runtime->stopSupplies(&stopError);
            if (!stopConfirmed)
                m_error+=QStringLiteral("; EMERGENCY STOP NOT CONFIRMED: ")+stopError;
        }
        // Never present the requested shutdown as confirmed if SDK stop failed.
        if (stopConfirmed) {m_master.setChecked(false);m_running=false;}
        return false;
    }
    m_touched=true;
    if (m_runtime && !m_runtime->configureSupplies(m_master.checked(),plus.enabled()==1,plus.volts(),
                                       minus.enabled()==1,minus.volts(),&m_error)) {
        QString stopError;
        const bool stopConfirmed=m_runtime->stopSupplies(&stopError);
        if (stopConfirmed) {m_master.setChecked(false);m_running=false;}
        else {
            // Hardware state is uncertain: do not report the supply as OFF.
            m_master.setChecked(true);
            m_running=true;
            m_error+=QStringLiteral("; EMERGENCY STOP NOT CONFIRMED: ")+stopError;
        }
        return false;
    }
    m_running=m_master.checked() && (plus.enabled()==1 || minus.enabled()==1);
    status(); // optional readback; status() does not erase configuration errors.
    return true;
}
bool PowerSuppliesBridge::run() {
    // An explicit call permits energizing the output. Setting a voltage alone does not.
    m_master.setChecked(true);
    return config();
}
bool PowerSuppliesBridge::stop() {
    m_error.clear();
    if (!m_touched) { // No output configuration was ever committed by this bridge.
        m_master.setChecked(false);
        m_running=false;
        return true;
    }
    if (m_runtime && !m_runtime->stopSupplies(&m_error)) {
        // Do not claim OFF when the SDK has rejected the stop command.
        return false;
    }
    m_master.setChecked(false);
    m_running=false;
    m_outputs.positiveChannel().setMeasured(0);
    m_outputs.negativeChannel().setMeasured(0);
    return true;
}
QVariantMap PowerSuppliesBridge::status() {
    const auto& plus=m_outputs.positiveChannel();
    const auto& minus=m_outputs.negativeChannel();
    QVariantMap result;
    result.insert(QStringLiteral("simulation"),m_runtime==nullptr);
    result.insert(QStringLiteral("masterEnabled"),m_master.checked());
    result.insert(QStringLiteral("positiveEnabled"),plus.enabled()==1);
    result.insert(QStringLiteral("negativeEnabled"),minus.enabled()==1);
    result.insert(QStringLiteral("positiveSetpoint"),plus.volts());
    result.insert(QStringLiteral("negativeSetpoint"),minus.volts());
    result.insert(QStringLiteral("running"),m_running);
    bool measured=false;
    double vp=0.0,vn=0.0;
    if (m_runtime) {
        QString readError;
        measured=m_runtime->suppliesReadback(&vp,&vn,&readError);
        if (!measured) result.insert(QStringLiteral("readbackError"),readError);
    } else {
        // Ideal simulated output, never confused with a physical measurement.
        vp=m_running && plus.enabled()==1 ? plus.volts():0.0;
        vn=m_running && minus.enabled()==1 ? minus.volts():0.0;
    }
    m_outputs.positiveChannel().setMeasured(vp);
    m_outputs.negativeChannel().setMeasured(vn);
    result.insert(QStringLiteral("measuredAvailable"),measured);
    result.insert(QStringLiteral("positiveVoltage"),m_runtime && !measured ? QVariant() : QVariant(vp));
    result.insert(QStringLiteral("negativeVoltage"),m_runtime && !measured ? QVariant() : QVariant(vn));
    result.insert(QStringLiteral("lastError"),m_error);
    return result;
}
