#!/usr/bin/env python3
"""Offline structural QA for Text/IP input revision; does not claim Qt runtime."""
import json,re,subprocess,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
PANEL=ROOT/'development/text_inputs/TextInputLab.wfpanel'

def check(cond,msg):
    if not cond:raise AssertionError(msg)

source=(ROOT/'src/widgets/LabeledTextInputWidget.cpp').read_text()
factory=(ROOT/'src/widgets/LabeledTextInputWidgetFactory.cpp').read_text()
registry=(ROOT/'src/widgets/ProfessionalInputFactories.cpp').read_text()
codec=(ROOT/'src/widgets/WidgetDocumentCodec.cpp').read_text()
ui=(ROOT/'src/PanelUiBridge.h').read_text()
schema=(ROOT/'src/PanelDataSchema.h').read_text()
designer=(ROOT/'src/PanelDesigner.cpp').read_text()
cmake=(ROOT/'CMakeLists.txt').read_text()
check('registerLabeledTextInputWidgets(registry);' in registry,'Factory registry')
check('"text_input","ip_address_input"' in designer,'Toolbox category')
for v in ('text_input','ip_address_input'):
    check(f'QStringLiteral("{v}")' in factory,'Factory type '+v)
    check(f'QStringLiteral("{v}")' in schema,'Panel Data type '+v)
    check(f'QStringLiteral("{v}")' in codec,'Document codec '+v)
for field in ('defaultText','placeholder','labelPosition','accent','maxLength','required','readOnly'):
    check(f'QStringLiteral("{field}")' in factory,'Configurable '+field)
for event in ('valueChanged','editingFinished','validationFailed'):
    check(f'QStringLiteral("{event}")' in factory,'Event '+event)
check('m_textInputRules' in ui and 'acceptsTextValue' in ui,'Worker-side input validation')
check('TextInputValidation::allowedValue' in (ROOT/'src/PanelProject.cpp').read_text(), 'Project initial validation')
check('m_editor->setReadOnly' in source and 'InstrumentVisuals::background' in source, 'Native styling')
check('WFScriptTextInputSmoke' in cmake, 'Native CTest wired')
check('LabeledTextInputWidget.cpp' in cmake,'CMake source wiring')

p=json.loads(PANEL.read_text())
check(p['format']=='WFScriptPanel' and p['version']==2,'Panel format')
check(len(p['pages'])==2, 'Two demonstration Pages')
check(len(p['panelData'])==2,'Panel Data variables')
ids=set();event_count=0;positions=set();types=set()
js=p['script']
for page in p['pages']:
    for c in page['controls']:
        check(c['id'] not in ids,'Unique IDs')
        ids.add(c['id']); types.add(c['type'])
        if c['type'] in ('text_input','ip_address_input'):
            pos=c['properties']['labelPosition']
            positions.add(pos)
            check(pos in ('Top','Bottom','Left','Right'),'Valid position')
            check(c['properties']['maxLength']>0,'Valid max length')
            if c['type']=='ip_address_input':
                v=c['properties']['defaultText']
                if v:check(len(v.split('.'))==4,'QA initial IPv4')
        for event,link in c.get('events',{}).items():
            event_count+=1
            check(link['enabled'],'Disabled QA event')
            check(re.search(r'\bfunction\s+'+re.escape(link['handler'])+r'\s*\(',js),
                  'Missing handler '+link['handler'])
        for name in c.get('bindings',{}).values():
            check(name in [v['name'] for v in p['panelData']],'Undefined Panel Data')
check(positions=={'Top','Bottom','Left','Right'},'All label positions demoed')
check({'text_input','ip_address_input'} <= types,'Both native types demonstrated')
check(event_count>=10,'Visual handlers demonstrated')
check('TCP.' not in js and 'Supplies.' not in js,'QA script uses no hardware')
subprocess.run(['node','--check',str(ROOT/'development/text_inputs/TextInputLab.js')],check=True)
subprocess.run(['node',str(ROOT/'tests/text_input_panel_mock.mjs')],check=True)
print('PASS: new native type factory/property/toolbox/binding contract, 2-page QA, %d event bindings, JS checks' % event_count)
