#!/usr/bin/env node
/*
 * WFScript educational JavaScript formatter (developer tool only).
 *
 * npm install --no-save typescript@5.8.3
 * node tools/format_educational_javascript.js
 *
 * This formatter inserts only whitespace, then runs TypeScript's formatter.
 * It rejects changes to the token stream or syntax tree. Comments and string
 * values are preserved; no algorithms, APIs, or generated data are changed.
 *
 * Third-party vendor modules are deliberately excluded.
 */
'use strict';
const fs = require('fs');
const path = require('path');
const crypto = require('crypto');
let ts;
try { ts = require('typescript'); }
catch (_e) {
    try {
        const globalModules = require('node:child_process').execFileSync('npm', ['root', '-g'], {encoding: 'utf8'}).trim();
        ts = require(path.join(globalModules, 'typescript'));
    } catch (_e2) {
        console.error('Install the development-only formatter dependency: npm install --no-save typescript@5.8.3');
        process.exit(2);
    }
}
const root = path.resolve(__dirname, '..');
const check = process.argv.includes('--check');
const show = process.argv.includes('--verbose');
const positions = [];
const formatOptions = {
    indentSize: 4, tabSize: 4, newLineCharacter: '\n', convertTabsToSpaces: true,
    insertSpaceAfterCommaDelimiter: true,
    insertSpaceAfterSemicolonInForStatements: true,
    insertSpaceBeforeAndAfterBinaryOperators: true,
    insertSpaceAfterKeywordsInControlFlowStatements: true,
    insertSpaceAfterFunctionKeywordForAnonymousFunctions: true,
    insertSpaceAfterOpeningAndBeforeClosingNonemptyBraces: true,
    placeOpenBraceOnNewLineForFunctions: false,
    placeOpenBraceOnNewLineForControlBlocks: false,
    semicolons: ts.SemicolonPreference.Insert
};
function sha(text) { return crypto.createHash('sha256').update(text).digest('hex'); }
function significantTokens(source) {
    const scanner = ts.createScanner(ts.ScriptTarget.ESNext, true, ts.LanguageVariant.Standard, source);
    const tokens = [];
    for (let kind = scanner.scan(); kind !== ts.SyntaxKind.EndOfFileToken; kind = scanner.scan()) {
        tokens.push([kind, scanner.getTokenText()]);
    }
    return tokens;
}
function syntaxShape(node, out) {
    out.push(node.kind);
    if (ts.isIdentifier(node)) out.push(node.text);
    else if (ts.isStringLiteral(node) || ts.isNumericLiteral(node)) out.push(node.text);
    ts.forEachChild(node, child => syntaxShape(child, out));
    out.push(-1);
}
function safeParse(file, source) {
    const sf = ts.createSourceFile(file, source, ts.ScriptTarget.ESNext, true, ts.ScriptKind.JS);
    if (sf.parseDiagnostics.length) {
        const d = sf.parseDiagnostics[0];
        const where = sf.getLineAndCharacterOfPosition(d.start || 0);
        throw Error(`${file}:${where.line + 1}:${where.character + 1} ${ts.flattenDiagnosticMessageText(d.messageText, '\n')}`);
    }
    return sf;
}
function format(file, source) {
    const original = safeParse(file, source);
    const inserts = new Map();
    function newline(at, count = 1) {
        if (at < 0 || at > source.length) return;
        inserts.set(at, Math.max(count, inserts.get(at) || 0));
    }
    function separated(items, afterOpening, beforeClosing) {
        if (!items.length) return;
        newline(afterOpening);
        newline(beforeClosing);
        for (let i = 0; i < items.length - 1; i++) {
            const between = source.slice(items[i].end, items[i + 1].getStart(original));
            const match = between.match(/^\s*,/);
            if (match) newline(items[i].end + match[0].length);
            else newline(items[i].end);
        }
    }
    function walk(node) {
        if (ts.isBlock(node) || ts.isModuleBlock(node) || ts.isCaseBlock(node)) {
            separated(node.statements, node.getStart(original) + 1, node.end - 1);
        }
        if (ts.isSourceFile(node)) {
            for (let i = 0; i < node.statements.length - 1; i++) {
                newline(node.statements[i].end, 2);
            }
        }
        if (ts.isCaseClause(node) || ts.isDefaultClause(node)) {
            const statements = node.statements;
            if (statements.length) {
                const colon = source.lastIndexOf(':', statements[0].getStart(original));
                if (colon >= 0) newline(colon + 1);
                for (let i = 0; i < statements.length - 1; i++) newline(statements[i].end);
            }
        }
        if (ts.isObjectLiteralExpression(node) && node.properties.length) {
            if (node.properties.length >= 2 || node.end - node.getStart(original) > 70) {
                separated(node.properties, node.getStart(original) + 1, node.end - 1);
            }
        }
        if (ts.isArrayLiteralExpression(node) && node.elements.length > 1 &&
            node.end - node.getStart(original) > 110) {
            separated(node.elements, node.getStart(original) + 1, node.end - 1);
        }
        if (ts.isVariableDeclarationList(node) && node.declarations.length > 1 &&
            ![ts.SyntaxKind.ForStatement, ts.SyntaxKind.ForInStatement, ts.SyntaxKind.ForOfStatement].includes(node.parent.kind)) {
            for (let i = 0; i < node.declarations.length - 1; i++) {
                const d = node.declarations;
                const between = source.slice(d[i].end, d[i + 1].getStart(original));
                const comma = between.match(/^\s*,/);
                if (comma) newline(d[i].end + comma[0].length);
            }
        }
        ts.forEachChild(node, walk);
    }
    walk(original);
    let expanded = source;
    for (const [at, count] of [...inserts.entries()].sort((a, b) => b[0] - a[0])) {
        const before = source.slice(0, at), after = source.slice(at);
        if (/\n[ \t]*$/.test(before) || /^[ \t]*\n/.test(after)) continue;
        expanded = expanded.slice(0, at) + '\n'.repeat(count) + expanded.slice(at);
    }
    const temp = file + '#format';
    const host = {
        getScriptFileNames: () => [temp], getScriptVersion: () => '0',
        getScriptSnapshot: p => p === temp ? ts.ScriptSnapshot.fromString(expanded) : undefined,
        getCurrentDirectory: () => root,
        getCompilationSettings: () => ({allowJs: true, target: ts.ScriptTarget.ES2020}),
        getDefaultLibFileName: () => '',
        fileExists: () => false, readFile: () => undefined, readDirectory: () => []
    };
    const languageService = ts.createLanguageService(host);
    const edits = languageService.getFormattingEditsForDocument(temp, formatOptions);
    languageService.dispose();
    let result = expanded;
    for (const edit of edits.sort((a, b) => b.span.start - a.span.start)) {
        result = result.slice(0, edit.span.start) + edit.newText + result.slice(edit.span.start + edit.span.length);
    }
    if (!result.endsWith('\n')) result += '\n';
    const beforeTokens = significantTokens(source), afterTokens = significantTokens(result);
    if (JSON.stringify(beforeTokens) !== JSON.stringify(afterTokens)) {
        const first = beforeTokens.findIndex((token, index) => JSON.stringify(token) !== JSON.stringify(afterTokens[index]));
        throw Error(`Token change forbidden in ${file}, token index ${first}`);
    }
    const formatted = safeParse(file, result);
    const beforeShape = [], afterShape = [];
    syntaxShape(original, beforeShape); syntaxShape(formatted, afterShape);
    if (JSON.stringify(beforeShape) !== JSON.stringify(afterShape)) {
        throw Error(`Syntax-tree change forbidden in ${file}`);
    }
    return {result, tokenSHA256: sha(JSON.stringify(beforeTokens))};
}
const sourceFiles = path.join(root, 'examples/source');
const libraryFiles = path.join(root, 'examples/libraries');
const compatibleFiles = path.join(root, 'examples/waveforms_compatible');
const entries = [];
function addFiles(dir, extension) {
    for (const filename of fs.readdirSync(dir).filter(n => n.endsWith(extension)).sort()) {
        entries.push(path.join(dir, filename));
    }
}
addFiles(sourceFiles, '.js');
addFiles(libraryFiles, '.mjs'); // EXCLUDES third-party /vendor modules
addFiles(compatibleFiles, '.js');
const manifest = {baseline: 'v0.2-dev.8.9.3.2', format: 'WFScript student-facing JS v1',
    description: 'Only whitespace and comments are allowed to change in this revision; identical TypeScript syntax-tree shapes and token streams were checked.',
    scripts: {}, modules: {}, waveformsCompatible: {}};
let totalBefore = 0, totalAfter = 0, changed = 0;
for (const file of entries) {
    const content = fs.readFileSync(file, 'utf8');
    const {result, tokenSHA256} = format(file, content);
    const rel = path.relative(root, file).replaceAll('\\', '/');
    const group = rel.includes('/source/') ? 'scripts' : rel.includes('/libraries/') ? 'modules' : 'waveformsCompatible';
    manifest[group][path.basename(file)] = {tokenSHA256, sha256: sha(result)};
    totalBefore += content.length; totalAfter += result.length;
    if (content !== result) {
        changed++;
        if (!check) fs.writeFileSync(file, result);
        if (show) console.log(`${rel}: ${content.length} -> ${result.length}`);
    }
}
const report = path.join(root, 'docs/EXAMPLE_JAVASCRIPT_FORMAT_LOCK.json');
if (!check) fs.writeFileSync(report, JSON.stringify(manifest, null, 2) + '\n');
console.log(`${check ? 'CHECK' : 'FORMAT'}: ${entries.length} first-party student-facing JavaScript files; ${changed} changed, ${totalBefore} -> ${totalAfter} characters; AST+token equivalence PASS`);
if (check && changed) process.exitCode = 1;
