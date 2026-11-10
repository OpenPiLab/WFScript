"""Source-only guard. Use CTest WFScriptKnowledgeSmoke for authoritative dynamic catalog validation."""
from pathlib import Path
import re
src = Path(__file__).resolve().parents[1] / 'src'
api=(src/'ApiKnowledge.cpp').read_text(encoding='utf-8')
editor=(src/'ScriptEditor.cpp').read_text(encoding='utf-8')
cm=(src.parent/'CMakeLists.txt').read_text(encoding='utf-8')
assert 'CompletionCatalog::entries(parent)' in api
assert 'if(!isDocumentedSymbol(p)) return out;' in api
assert 'for(const auto& d:kDetails)' in api
assert 'ApiKnowledge::lookup(' in editor
assert 'showApiHelp()' in editor
assert 'nextSnippetStop()' in editor
assert 'checkInventory(pair.first,pair.second)' in (src.parent/'tests/qt_api_smoke.cpp').read_text()
assert 'src/ApiKnowledge.cpp' in cm
assert 'WFScriptKnowledgeSmoke' in cm
assert 'WFScriptEditorKnowledgeSmoke' in cm
paths = re.findall(r'^    \{"(.*?)",\s*"', api, flags=re.M)
assert len(paths)>=20, f'Only {len(paths)} curated docs found'
assert len(set(paths))==len(paths), 'Duplicate curated docs'
for name in ('wgsine','scopecapture','scopefft','scopewindow','scoperef','checkdevice'):
    assert name in api, f'Missing snippet {name}'
print(f'PASS: {len(paths)} curated doc paths, guarded lookup, help and snippets are registered')
