#!/usr/bin/env python3
"""RC1.5.14.10.2: Guards against the 17 MinGW warnings in the owner's Debug build log.

This is source verification, not a substitute for recompiling with Qt 6.9.2.
"""
from pathlib import Path
import re

root=Path(__file__).resolve().parents[1]
def source(name):
    return (root/name).read_text(encoding='utf-8')

builtin=source('src/widgets/BuiltinWidgets.cpp')
assert 'void applyRuntimeValue(QWidget* w,const PanelControl& /* definition */,const QVariant& value) const override {' in builtin
csv=source('src/widgets/ExperimentWidgets.cpp')
assert csv.count('''if(error) {\n            *error=file.errorString();\n        }\n        return false;''')==3
meas=source('src/widgets/MeasurementTableWidget.cpp')
assert 'if(!file.open(QIODevice::WriteOnly|QIODevice::Text)) {\n        if(error) {\n            *error=file.errorString();\n        }\n        return false;\n    }' in meas
assert 'file.cancelWriting();\n        return false;' in meas
binary=source('src/BinaryBridge.cpp')
for bad in (
    'for(int i=0;i<count;++i)result^=static_cast<unsigned char>(p[i]);return result;',
    'if(m_error.isEmpty())m_error=QStringLiteral("Invalid sample count, scale or range");return nullptr;',
    'for(int i=0;i<count;++i)result.setProperty(quint32(i),m_values[offset+i]);return result;',
):
    assert bad not in binary
assert 'result^=static_cast<unsigned char>(p[i]);\n    }\n    return result;' in binary
assert 'result.setProperty(quint32(i),m_values[offset+i]);\n    }\n    return result;' in binary
protocol=source('src/DwfProtocols.cpp')
for bad in (
    'if(!ok)return false;output=byteList(r);',
    'if(!ok)return false;for(auto w:r)output.append(uint(w));',
    'if(!ok)return false;for(auto w:r)output.append(qulonglong(w));',
    'if(error)*error=QStringLiteral("Classical CAN: ID 11/29-bit, DLC 0..8, remote frames carry no payload");return false;',
):
    assert bad not in protocol
for message in (
    'UART: dataBits 5..8, parity 0..4, polarity 0/1 required',
    'SPI mode must be 0..3 and duty must be an SDK-supported setting',
    'SPI bits 1..32, lanes 0/1/2/4 and readCount 0..65536 required',
    'SPI command supports 1..8 command bits and 0..1024 dummy bits',
    'Classical CAN: ID 11/29-bit, DLC 0..8, remote frames carry no payload',
):
    assert f'*error=QStringLiteral("{message}");' in protocol
assert protocol.count('if(!ok) {\n                    return false;\n                }')==3
print('PASS RC1.5.14.10.2: all 17 logged warning sites corrected (source contract)')
