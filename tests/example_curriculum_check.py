#!/usr/bin/env python3
"""Verify the educational revision cannot silently change tested Panel behavior.

Run: python tests/example_curriculum_check.py
No Qt, no external packages, no physical device required.
"""
from pathlib import Path
import hashlib,json,re,sys

R=Path(__file__).resolve().parents[1]; E=R/'examples'
MANIFEST=json.loads((R/'docs/EXAMPLE_EDUCATION_BASELINE_LOCK.json').read_text(encoding='utf8'))
FORMAT=json.loads((R/'docs/EXAMPLE_JAVASCRIPT_FORMAT_LOCK.json').read_text(encoding='utf8'))
CAT=json.loads((E/'catalog.json').read_text(encoding='utf8'))
# The 36 source-locked curriculum Panels remain frozen; additional later labs
# (Panel Data, DSP Studio, etc.) are allowed without weakening the baseline.
assert len(CAT)>=MANIFEST['expectedExampleCount']==36
CAT=[entry for entry in CAT if entry['file'] in MANIFEST['scripts']]
assert len(CAT)==36
GENERIC='// VISUAL EVENTS: Select a widget in Design mode > Properties > Events > Go to Handler.\n// The event assignments live in the .wfpanel; do not register the same events twice.\n'
SHA=lambda b:hashlib.sha256(b).hexdigest()

def check(cond,problem):
 if not cond: raise AssertionError(problem)

# Only these five user-facing Label texts changed in the two teaching Panels.
OLD_LABELS={
 'GettingStarted':{
  'docsIntro':'Explore focused, editable examples from the Examples menu on the Panel toolbar. Each example is a normal .wfpanel project with its own documented JavaScript.',
  'docsStructure':'GET STARTED\n\n1. Open Page 02 Events Tutorial.\n2. Select the button in Design mode.\n3. In Properties > Events, choose clicked.\n4. Use Go to Handler to inspect its JavaScript.\n5. Press Run Panel and click the button.',
  'docsDesigner':'MULTI-PAGE PROJECTS\n\nWidget Playground, Analog Charts and Chart Techniques contain multiple Pages. Their widgets share one script runtime, so switching Pages does not restart measurements.\n\nUse Properties and Design Tools to customize the layout.',
  'docsFooter':'TIP: Examples are bundled as read-only Qt resources. Save/Export creates a normal editable .wfpanel file.',
 },
 'WidgetPlayground':{
  'tourBox':'HOW TO USE\n\n1. Click Run Panel.\n2. Switch to the Controls Page and change the inputs.\n3. Return here: values persist across Pages.\n4. Click Edit JavaScript to study wbRefresh() and ui.on().',
 }
}

bindings=0
for entry in CAT:
 stem=entry['file']; jsfile=E/'source'/f'{stem}.js'; pfile=E/'panels'/f'{stem}.wfpanel'; guide=E/'guides'/f'{stem}.md'
 check(jsfile.exists() and pfile.exists() and guide.exists(),f'missing example component: {stem}')
 js=jsfile.read_text(encoding='utf8'); proj=json.loads(pfile.read_text(encoding='utf8')); doc=guide.read_text(encoding='utf8')
 check(js.startswith('// ┌─ WFScript LEARNING LAB:'),f'no embedded learning header: {stem}')
 check('GOAL:' in js and 'TRY FIRST:' in js and 'TRACE:' in js and 'EDIT SAFELY:' in js,f'incomplete learning header: {stem}')
 check(f'examples/guides/{stem}.md' in js,f'guide not referenced: {stem}')
 check('## Follow along' in doc and '## Make one small change' in doc and '## Safety and scope' in doc,f'incomplete guide: {stem}')
 check(proj['script']==js,f'embedded script out of sync: {stem}')
 # v8.9.4 deliberately reformatted these source-locked learning examples.
 # Their pre-format checksums remain in EXAMPLE_EDUCATION_BASELINE_LOCK.json;
 # all executable tokens and syntax trees were verified unchanged, and new
 # formatted-file SHA locks prevent later accidental functional changes.
 m=re.match(r'^// ┌─ WFScript LEARNING LAB:.*?^// └[^\n]*\n\n',js,flags=re.M|re.S)
 check(m is not None,f'cannot parse learning header: {stem}')
 check('CORE LESSON' in js if stem!='EducationalVisuals' else True,f'no function-level explanatory comment: {stem}')
 check(SHA(js.encode('utf8'))==FORMAT['scripts'][stem+'.js']['sha256'],f'FORMATTED SCRIPT CHANGED: {stem}')
 saved=0
 for page in proj['pages']:
  for ctrl in page.get('controls',[]):
   for evt,bind in (ctrl.get('events') or {}).items():
    if not isinstance(bind,dict) or not bind.get('enabled'):continue
    saved+=1
    handler=bind.get('handler','')
    check(bool(handler) and bool(re.search(r'\bfunction\s+'+re.escape(handler)+r'\s*\(',js)),f'{stem}/{ctrl["id"]}/{evt}: missing function {handler}')
    check(f'`{ctrl["id"]}`' in doc and f'`{handler}()`' in doc,f'{stem}/{ctrl["id"]} event undocumented')
 bindings+=saved
 if stem=='EducationalVisuals':check(saved==0,'educational graphics must remain static')
 # Check that no coordinates, event metadata, test definitions or other
 # non-script project content changed (except the five intentional labels).
 layout=dict(proj);layout.pop('script',None);layout.pop('libraries',None)
 for pg in layout.get('pages',[]):
  for ctrl in pg.get('controls',[]):
   previous=OLD_LABELS.get(stem,{}).get(ctrl.get('id'))
   if previous is not None:ctrl['text']=previous
 # RC1.5.14.6.1 explicitly preserved legacy valueChanged(rows) on the four
 # previously editable public lab tables. The educational v0.2-dev.7.5
 # project-structure lock predates these exact compatibility-only properties.
 # Require each known opt-in property to remain true, remove only those fields
 # for the legacy comparison, and retain the original immutable checksums.
 compatible_tables={
  'CalibrationTable':('points','calibrationTable'),
  'EquationExperimentLab':('observations','studentTable'),
  'TableDrivenWavegen':('recipe','recipeTable'),
  'TableLimitPlot':('edit','limitInputTable'),
 }
 compatibility_fields=[(pg['id'],c.get('id'),c['properties']['emitFullChangeEvent'],c)
                      for pg in layout['pages'] for c in pg['controls']
                      if 'emitFullChangeEvent' in c.get('properties',{})]
 expected=compatible_tables.get(stem)
 if expected:
  check(len(compatibility_fields)==1 and
        compatibility_fields[0][:2]==expected and
        compatibility_fields[0][2] is True,
        f'{stem}: legacy editable-table event compatibility changed')
  compatibility_fields[0][3]['properties'].pop('emitFullChangeEvent')
 else:
  check(not compatibility_fields,
        f'{stem}: unexpected compatibility flag in frozen example layout')
 check(SHA(json.dumps(layout,ensure_ascii=False,sort_keys=True).encode())==MANIFEST['projectStructure'][stem],f'PROJECT BEHAVIOR OR LAYOUT CHANGED: {stem}')
 print(f'PASS {stem:27} {saved:2} designer bindings / guide and script locked')

check(bindings==MANIFEST['expectedVisualBindings'],f'visual bindings changed: {bindings}')
for filename,expected in MANIFEST['modules'].items():
 s=(E/'libraries'/filename).read_text(encoding='utf8')
 # The six historical modules were reformatted without changing JS tokens.
 # The original baseline remains in MANIFEST; the formatted SHA is locked here.
 check(filename in FORMAT['modules'],f'missing formatted module lock: {filename}')
 check(SHA(s.encode('utf8'))==FORMAT['modules'][filename]['sha256'],f'FORMATTED MODULE CHANGED: {filename}')
check('setWindowTitle(QStringLiteral("WFScript v0.2"))' in (R/'src/MainWindow.cpp').read_text(), 'Window version not updated')
check((E/'YOUR_FIRST_PANEL.md').exists() and (E/'LEARN_WFSCRIPT.md').exists(),'missing learning path')
print(f'PASS: {len(CAT)} source-locked examples, {bindings} unchanged bindings, {len(MANIFEST["modules"])} source-locked .mjs modules, 36 lab guides')
