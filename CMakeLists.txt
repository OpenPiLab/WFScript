cmake_minimum_required(VERSION 3.21)
project(WFScript VERSION 0.2.0 LANGUAGES CXX)

# Windows application icon uses the platform resource compiler (windres/rc.exe).
if(WIN32)
    enable_language(RC)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Widgets Qml Sql Network SerialPort)

set(WFSCRIPT_WIDGET_SOURCES
    src/plot/PlotCommand.h
    src/plot/NativeSampleIndex.h
    src/plot/PlotPixelColumns.h
    src/plot/PlotAxisLabels.h
    src/plot/LegacyXYIndex.h
    src/plot/DigitalEdgeIndex.h
    src/plot/PanelChartWidget.h
    src/plot/PanelChartWidget.cpp
    src/plot/PanelPlotFactory.h
    src/plot/PanelPlotFactory.cpp
    src/plot/DigitalTimingPlotFactory.h
    src/plot/DigitalTimingPlotFactory.cpp
    src/widgets/WidgetDefinition.h
    src/widgets/WidgetFactory.h
    src/widgets/WidgetRegistry.h
    src/widgets/WidgetRegistry.cpp
    src/widgets/BuiltinWidgets.h
    src/widgets/BuiltinWidgets.cpp
    src/widgets/EducationWidgets.h
    src/widgets/EducationWidgets.cpp
    src/widgets/ExperimentTableData.h
    src/widgets/ExperimentTableModel.h
    src/widgets/ExperimentTableModel.cpp
    src/widgets/ExperimentWidgets.h
    src/widgets/ExperimentWidgets.cpp
    src/widgets/RunInformationWidget.h
    src/widgets/RunInformationWidget.cpp
    src/widgets/PanelGauge.h
    src/widgets/PanelGauge.cpp
    src/widgets/PanelLabelWidget.h
    src/widgets/PanelLabelWidget.cpp
    src/widgets/PanelSliderWidget.h
    src/widgets/PanelSliderWidget.cpp
    src/widgets/DigitalDisplayWidget.h
    src/widgets/DigitalDisplayWidget.cpp
    src/widgets/DigitalDisplayWidgetFactory.h
    src/widgets/DigitalDisplayWidgetFactory.cpp
    src/widgets/MeasurementResultWidget.h
    src/widgets/MeasurementResultWidget.cpp
    src/widgets/MeasurementResultWidgetFactory.h
    src/widgets/MeasurementResultWidgetFactory.cpp
    src/widgets/MeasurementTableWidget.h
    src/widgets/MeasurementTableWidget.cpp
    src/widgets/MeasurementTableWidgetFactory.h
    src/widgets/MeasurementTableWidgetFactory.cpp
    src/widgets/TestSequenceWidget.h
    src/widgets/TestSequenceWidget.cpp
    src/widgets/TestSequenceWidgetFactory.h
    src/widgets/TestSequenceWidgetFactory.cpp
    src/widgets/EventLogWidget.h
    src/widgets/EventLogWidget.cpp
    src/widgets/EventLogWidgetFactory.h
    src/widgets/EventLogWidgetFactory.cpp
    src/widgets/UartTerminalWidget.h
    src/widgets/UartTerminalWidget.cpp
    src/widgets/UartTerminalWidgetFactory.h
    src/widgets/UartTerminalWidgetFactory.cpp
    src/widgets/CommunicationsTerminalWidget.h
    src/widgets/CommunicationsTerminalWidget.cpp
    src/widgets/CommunicationsTerminalWidgetFactory.h
    src/widgets/CommunicationsTerminalWidgetFactory.cpp
    src/widgets/PanelThemedCheckBox.h
    src/widgets/InstrumentVisuals.h
    src/widgets/BasicWidgetVisuals.h
    src/widgets/PrecisionDialWidget.h
    src/widgets/PrecisionDialWidget.cpp
    src/widgets/PrecisionDialWidgetFactory.h
    src/widgets/PrecisionDialWidgetFactory.cpp
    src/widgets/StatusBeaconWidget.h
    src/widgets/StatusBeaconWidget.cpp
    src/widgets/StatusBeaconWidgetFactory.h
    src/widgets/StatusBeaconWidgetFactory.cpp
    src/widgets/SegmentedBarWidget.h
    src/widgets/SegmentedBarWidget.cpp
    src/widgets/SegmentedBarWidgetFactory.h
    src/widgets/SegmentedBarWidgetFactory.cpp
    src/widgets/InstrumentButtonWidget.h
    src/widgets/InstrumentButtonWidget.cpp
    src/widgets/RotaryInputWidget.h
    src/widgets/RotaryInputWidget.cpp
    src/widgets/NumericStepperWidget.h
    src/widgets/NumericStepperWidget.cpp
    src/widgets/OptionSelectorWidget.h
    src/widgets/OptionSelectorWidget.cpp
    src/widgets/StatusBannerWidget.h
    src/widgets/StatusBannerWidget.cpp
    src/widgets/ProfessionalInputFactories.h
    src/widgets/ProfessionalInputFactories.cpp
    src/widgets/TextInputValidation.h
    src/widgets/LabeledTextInputWidget.h
    src/widgets/LabeledTextInputWidget.cpp
    src/widgets/LabeledTextInputWidgetFactory.h
    src/widgets/LabeledTextInputWidgetFactory.cpp
    src/widgets/WidgetInitialValues.h
    src/widgets/WidgetInitialValues.cpp
    src/widgets/WidgetDocumentCodec.h
    src/widgets/WidgetDocumentCodec.cpp
    src/widgets/WidgetPropertyInspector.h
    src/widgets/WidgetPropertyInspector.cpp
)

qt_add_executable(WFScript
    src/resources/icons.qrc
    src/resources/examples.qrc
    src/UiIcons.h
    src/PanelEventCatalog.h
    src/PanelCanvas.h
    src/PanelCanvas.cpp
    ${WFSCRIPT_WIDGET_SOURCES}
    src/panel/PanelAppearance.h
    src/panel/PanelAppearanceColors.h
    src/panel/PageAppearanceEditor.h
    src/panel/PageAppearanceEditor.cpp
    src/panel/ScopeMetricsFormatter.h
    src/panel/ScopeMetricsFormatter.cpp
    src/PanelDesigner.h
    src/PanelDesigner.cpp
    src/TestSequenceEditor.h
    src/TestSequenceEditor.cpp
    src/RunInformationSchema.h
    src/RunInformationEditor.h
    src/RunInformationEditor.cpp
    src/PanelProject.h
    src/PanelProject.cpp
    src/PanelRuntime.h
    src/PanelRuntime.cpp
    src/ScriptTimerManager.h
    src/ScriptTimerManager.cpp
    src/ScriptUtilityBridge.h
    src/ScriptUtilityBridge.cpp
    src/SqliteBridge.h
    src/SqliteBridge.cpp
    src/CommunicationBridge.h
    src/CommunicationBridge.cpp
    src/BinaryBridge.h
    src/BinaryBridge.cpp
    src/PanelUiBridge.h
    src/PanelDataEvents.h
    src/ScriptValueConversion.h
    src/TestRunnerBridge.h
    src/TestRunnerBridge.cpp
    src/TestReportStorage.h
    src/TestResultsRepository.h
    src/TestResultsRepository.cpp
    src/TestResultsHistory.cpp
    src/TestResultsBrowserDialog.h
    src/TestResultsBrowserDialog.cpp
    src/TestResultsTableModels.h src/TestResultsTableModels.cpp
    src/plot/PlotBridge.h
    src/plot/PlotBridge.cpp
    src/ScriptLibraryBridge.h
    src/ScriptLibraryBridge.cpp
    src/ScriptErrorDetails.h
    src/ScriptErrorDetails.cpp
    src/main.cpp
    src/device/DeviceWorkspace.h
    src/device/DeviceWorkspace.cpp
    src/MainWindow.h
    src/MainWindow.cpp
    src/ScriptEditor.h
    src/ScriptEditor.cpp
    src/EditorAssist.h
    src/EditorAssist.cpp
    src/JavaScriptHighlighter.h
    src/JavaScriptHighlighter.cpp
    src/ScriptHelpers.h
    src/ScriptWorker.h
    src/ScriptWorker.cpp
    src/DwfRuntime.h
    src/DwfRuntime.cpp
    src/DwfProtocols.cpp
    src/WaveFormsBindings.h
    src/WaveFormsBindings.cpp
    src/PowerSuppliesBridge.h
    src/PowerSuppliesBridge.cpp
    src/StaticIOBridge.h
    src/StaticIOBridge.cpp
    src/PatternsBridge.h
    src/PatternsBridge.cpp
    src/ProtocolBridge.h
    src/ProtocolBridge.cpp
    src/CompletionCatalog.h
    src/CompletionCatalog.cpp
    src/ApiKnowledge.h
    src/ApiKnowledge.cpp
    src/ApiDiagnostics.h
    src/ApiDiagnostics.cpp
    src/ApiExplorer.h
    src/ApiExplorer.cpp
    src/CompletionTypeResolver.h
    src/CompletionTypeResolver.cpp
    src/ModuleExportCatalog.h
    src/ModuleExportCatalog.cpp
    src/ScopeTrigger.h
    src/ScopeFrame.h
    src/ScopeMath.h
    src/ScopeMath.cpp
    src/ScopeAnalysis.h
    src/ScopeAnalysis.cpp
    src/ScopeAcquisitionWorker.h
    src/ScopeAcquisitionWorker.cpp
    src/WaveformPlot.h
    src/WaveformPlot.cpp
)

if(WIN32)
    target_sources(WFScript PRIVATE src/resources/wfscript.rc)
endif()

target_include_directories(WFScript PRIVATE src)
target_link_libraries(WFScript PRIVATE Qt6::Widgets Qt6::Qml Qt6::Sql Qt6::Network Qt6::SerialPort)
if(MSVC)
    target_compile_options(WFScript PRIVATE /utf-8 /W4)
else()
    target_compile_options(WFScript PRIVATE -Wall -Wextra -Wpedantic)
endif()
set_target_properties(WFScript PROPERTIES
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
)

# Not a Digilent product; this is an independent implementation.

# Three headless PanelRuntime smoke tests build the *actual* runtime, not a
# stub. PanelRuntime.cpp instantiates the script worker, the native Test
# Executive, StaticIO, Patterns and PanelUiBridge. Their implementations and
# Q_OBJECT-bearing headers must participate in EACH test target; merely
# including these headers from PanelRuntime.cpp does not generate the needed
# Qt moc_*.cpp objects. Omitting them produces undefined references at link.
#
# Keep one explicit dependency list instead of silently diverging test targets.
set(WFSCRIPT_PANEL_RUNTIME_SMOKE_DEPENDENCIES
    src/ScriptTimerManager.h
    src/ScriptTimerManager.cpp
    src/ScriptUtilityBridge.h
    src/ScriptUtilityBridge.cpp
    src/SqliteBridge.h
    src/SqliteBridge.cpp
    src/CommunicationBridge.h
    src/CommunicationBridge.cpp
    src/BinaryBridge.h
    src/BinaryBridge.cpp
    src/PanelUiBridge.h             # header-only QObject; AUTOMOC generates staticMetaObject
    src/PanelDataEvents.h
    src/ScriptValueConversion.h
    src/ScriptWorker.h
    src/ScriptWorker.cpp
    src/TestRunnerBridge.h
    src/TestRunnerBridge.cpp
    src/TestReportStorage.h
    src/TestResultsRepository.h
    src/TestResultsRepository.cpp
    src/RunInformationSchema.h
    src/StaticIOBridge.h
    src/StaticIOBridge.cpp
    src/PatternsBridge.h
    src/PatternsBridge.cpp
    src/ProtocolBridge.h
    src/ProtocolBridge.cpp
)

# Optional lightweight simulation tests. The test executable does not launch
# QWidget UI, connect to an AD3 or require a Digilent DLL.
option(WFSCRIPT_BUILD_TESTS "Build native Qt smoke/regression tests (no AD3 required)" OFF)
if(WFSCRIPT_BUILD_TESTS)
    enable_testing()
    qt_add_executable(WFScriptApiSmoke
        tests/qt_api_smoke.cpp
        src/WaveFormsBindings.h
        src/WaveFormsBindings.cpp
        src/ScriptHelpers.h
        src/ScopeFrame.h
        src/ScopeTrigger.h
        src/ScopeMath.h
        src/ScopeMath.cpp
        src/ScopeAnalysis.h
        src/ScopeAnalysis.cpp
        src/DwfRuntime.h
        src/DwfRuntime.cpp
        src/DwfProtocols.cpp
        src/CompletionCatalog.h
        src/CompletionCatalog.cpp
        src/ApiKnowledge.h
        src/ApiKnowledge.cpp
    )
    target_include_directories(WFScriptApiSmoke PRIVATE src)
    target_link_libraries(WFScriptApiSmoke PRIVATE Qt6::Core Qt6::Qml)
    add_test(NAME WFScriptApiSmoke COMMAND WFScriptApiSmoke)
endif()


# v0.2-dev: ProtocolBridge C++/QJSEngine API contracts in Simulation.
# Native AD3 and SDK ABI validation are separate physical tests.
if(WFSCRIPT_BUILD_TESTS)
    qt_add_executable(WFScriptProtocolBridgeSmoke
        tests/protocol_bridge_smoke.cpp
        src/ProtocolBridge.h src/ProtocolBridge.cpp
        src/DwfRuntime.h src/DwfRuntime.cpp src/DwfProtocols.cpp
        src/CompletionCatalog.h src/CompletionCatalog.cpp
        src/ApiKnowledge.h src/ApiKnowledge.cpp
        src/ScopeMath.h src/ScopeMath.cpp
        src/ScopeAnalysis.h src/ScopeAnalysis.cpp)
    target_include_directories(WFScriptProtocolBridgeSmoke PRIVATE src)
    target_link_libraries(WFScriptProtocolBridgeSmoke PRIVATE Qt6::Core Qt6::Qml)
    add_test(NAME WFScriptProtocolBridgeSmoke COMMAND WFScriptProtocolBridgeSmoke)
endif()

# v0.2-dev.1: actual C++ native UART queue, callback and Terminal tests.
# A headless Qt Widgets fixture injects synthetic bytes; no DWF hardware.
if(WFSCRIPT_BUILD_TESTS)
    qt_add_executable(WFScriptUartAsyncSmoke
        tests/uart_async_smoke.cpp
        src/ProtocolBridge.h src/ProtocolBridge.cpp
        src/DwfRuntime.h src/DwfRuntime.cpp src/DwfProtocols.cpp
        src/CompletionCatalog.h src/CompletionCatalog.cpp
        src/ApiKnowledge.h src/ApiKnowledge.cpp
        src/ScopeMath.h src/ScopeMath.cpp
        src/ScopeAnalysis.h src/ScopeAnalysis.cpp
        src/widgets/UartTerminalWidget.h src/widgets/UartTerminalWidget.cpp
        src/widgets/UartTerminalWidgetFactory.h src/widgets/UartTerminalWidgetFactory.cpp)
    target_include_directories(WFScriptUartAsyncSmoke PRIVATE src)
    target_link_libraries(WFScriptUartAsyncSmoke PRIVATE Qt6::Widgets Qt6::Qml)
    add_test(NAME WFScriptUartAsyncSmoke COMMAND WFScriptUartAsyncSmoke)
    set_tests_properties(WFScriptUartAsyncSmoke PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()

# v0.2-dev.2: real QJSEngine + native QTimer lifecycle and JS API smoke.
if(WFSCRIPT_BUILD_TESTS)
    qt_add_executable(WFScriptTimerSmoke
        tests/script_timer_smoke.cpp
        src/ScriptTimerManager.h src/ScriptTimerManager.cpp
        src/CompletionCatalog.h src/CompletionCatalog.cpp
        src/ApiKnowledge.h src/ApiKnowledge.cpp
        src/EditorAssist.h src/EditorAssist.cpp)
    target_include_directories(WFScriptTimerSmoke PRIVATE src)
    target_link_libraries(WFScriptTimerSmoke PRIVATE Qt6::Core Qt6::Qml)
    add_test(NAME WFScriptTimerSmoke COMMAND WFScriptTimerSmoke)
    qt_add_executable(WFScriptUtilitySmoke
        tests/ScriptUtilitySmoke.cpp
        src/ScriptUtilityBridge.h src/ScriptUtilityBridge.cpp
        src/BinaryBridge.h src/BinaryBridge.cpp)
    target_include_directories(WFScriptUtilitySmoke PRIVATE src)
    target_link_libraries(WFScriptUtilitySmoke PRIVATE Qt6::Core Qt6::Qml)
    add_test(NAME WFScriptUtilitySmoke COMMAND WFScriptUtilitySmoke)
    set_tests_properties(WFScriptUtilitySmoke PROPERTIES TIMEOUT 10)

    set_tests_properties(WFScriptTimerSmoke PROPERTIES TIMEOUT 10)
endif()

# v0.2-dev.4: native opt-in StaticIO monitor and QJSEngine callback test.
# CTest runs entirely in simulation; physical Scope/Wavegen coexistence is a
# separate operator test on a real AD3, not an automated CI claim.
if(WFSCRIPT_BUILD_TESTS)
    qt_add_executable(WFScriptStaticIOMonitorSmoke
        tests/static_io_monitor_smoke.cpp
        src/StaticIOBridge.h src/StaticIOBridge.cpp
        src/DwfRuntime.h src/DwfRuntime.cpp src/DwfProtocols.cpp
        src/ScopeMath.h src/ScopeMath.cpp
        src/ScopeAnalysis.h src/ScopeAnalysis.cpp
        src/CompletionCatalog.h src/CompletionCatalog.cpp
        src/ApiKnowledge.h src/ApiKnowledge.cpp)
    target_include_directories(WFScriptStaticIOMonitorSmoke PRIVATE src)
    target_link_libraries(WFScriptStaticIOMonitorSmoke PRIVATE Qt6::Core Qt6::Qml)
    add_test(NAME WFScriptStaticIOMonitorSmoke COMMAND WFScriptStaticIOMonitorSmoke)
    set_tests_properties(WFScriptStaticIOMonitorSmoke PROPERTIES TIMEOUT 10)
endif()

# Portable standalone numerical reference (no DWF hardware needed).
if(WFSCRIPT_BUILD_TESTS)
    add_executable(WFScriptDspReference
        tests/scope_analysis_reference.cpp
        src/ScopeAnalysis.cpp
        src/ScopeAnalysis.h)
    target_include_directories(WFScriptDspReference PRIVATE src)
    add_test(NAME WFScriptDspReference COMMAND WFScriptDspReference)
endif()

# Editor-only Qt highlighter regression: no AD3 or DWF DLL required.
if(WFSCRIPT_BUILD_TESTS)
    qt_add_executable(WFScriptHighlightSmoke
        tests/highlighter_smoke.cpp
        src/JavaScriptHighlighter.h
        src/JavaScriptHighlighter.cpp)
    target_include_directories(WFScriptHighlightSmoke PRIVATE src)
    target_link_libraries(WFScriptHighlightSmoke PRIVATE Qt6::Gui Qt6::Core)
    add_test(NAME WFScriptHighlightSmoke COMMAND WFScriptHighlightSmoke)
endif()

# Static completion inference tests; no QWidget, QJSEngine or DWF hardware.
if(WFSCRIPT_BUILD_TESTS)
    qt_add_executable(WFScriptCompletionSmoke
        tests/completion_types_smoke.cpp
        src/CompletionTypeResolver.h
        src/CompletionTypeResolver.cpp
    src/ModuleExportCatalog.h
    src/ModuleExportCatalog.cpp
        src/CompletionCatalog.h
        src/CompletionCatalog.cpp)
    target_include_directories(WFScriptCompletionSmoke PRIVATE src)
    target_link_libraries(WFScriptCompletionSmoke PRIVATE Qt6::Core)
    add_test(NAME WFScriptCompletionSmoke COMMAND WFScriptCompletionSmoke)
endif()

# QWidget-based completion popup interaction tests (offscreen-capable).
# Covers first-character filtering, highlighted selection and backspace recovery.
if(WFSCRIPT_BUILD_TESTS)
    qt_add_executable(WFScriptCompletionPopupSmoke
        tests/completion_popup_smoke.cpp
        src/EditorAssist.h
        src/EditorAssist.cpp
        src/ScriptEditor.h
        src/ScriptEditor.cpp
        src/ApiDiagnostics.h src/ApiDiagnostics.cpp
        src/JavaScriptHighlighter.h
        src/JavaScriptHighlighter.cpp
        src/CompletionCatalog.h
        src/CompletionCatalog.cpp
        src/ApiKnowledge.h src/ApiKnowledge.cpp
        src/CompletionTypeResolver.h
        src/CompletionTypeResolver.cpp
        src/ModuleExportCatalog.h src/ModuleExportCatalog.cpp)
    target_include_directories(WFScriptCompletionPopupSmoke PRIVATE src)
    target_link_libraries(WFScriptCompletionPopupSmoke PRIVATE Qt6::Widgets)
    add_test(NAME WFScriptCompletionPopupSmoke COMMAND WFScriptCompletionPopupSmoke)
    set_tests_properties(WFScriptCompletionPopupSmoke PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()

# Smart editing and API hint tests; offscreen Qt Widgets, no hardware.
if(WFSCRIPT_BUILD_TESTS)
    qt_add_executable(WFScriptSmartEditingSmoke
        tests/smart_editing_smoke.cpp
        src/EditorAssist.h src/EditorAssist.cpp
        src/ScriptEditor.h src/ScriptEditor.cpp
        src/ModuleExportCatalog.h src/ModuleExportCatalog.cpp
        src/ApiDiagnostics.h src/ApiDiagnostics.cpp
        src/JavaScriptHighlighter.h src/JavaScriptHighlighter.cpp
        src/CompletionCatalog.h src/CompletionCatalog.cpp
        src/ApiKnowledge.h src/ApiKnowledge.cpp
        src/CompletionTypeResolver.h src/CompletionTypeResolver.cpp)
    target_include_directories(WFScriptSmartEditingSmoke PRIVATE src)
    target_link_libraries(WFScriptSmartEditingSmoke PRIVATE Qt6::Widgets)
    add_test(NAME WFScriptSmartEditingSmoke COMMAND WFScriptSmartEditingSmoke)
    set_tests_properties(WFScriptSmartEditingSmoke PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()

# API catalog and documentation coherence (QtCore only; no AD3, no QJSEngine).
if(WFSCRIPT_BUILD_TESTS)
    qt_add_executable(WFScriptKnowledgeSmoke
        tests/api_knowledge_smoke.cpp
        src/CompletionCatalog.h src/CompletionCatalog.cpp
        src/ApiKnowledge.h src/ApiKnowledge.cpp)
    target_include_directories(WFScriptKnowledgeSmoke PRIVATE src)
    target_link_libraries(WFScriptKnowledgeSmoke PRIVATE Qt6::Core)
    add_test(NAME WFScriptKnowledgeSmoke COMMAND WFScriptKnowledgeSmoke)
endif()

# Editor documentation and snippets: popup-independent tests (offscreen-capable).
if(WFSCRIPT_BUILD_TESTS)
    qt_add_executable(WFScriptEditorKnowledgeSmoke
        tests/editor_knowledge_smoke.cpp
        src/EditorAssist.h src/EditorAssist.cpp
        src/ScriptEditor.h src/ScriptEditor.cpp
        src/ModuleExportCatalog.h src/ModuleExportCatalog.cpp
        src/ApiDiagnostics.h src/ApiDiagnostics.cpp
        src/JavaScriptHighlighter.h src/JavaScriptHighlighter.cpp
        src/CompletionCatalog.h src/CompletionCatalog.cpp
        src/ApiKnowledge.h src/ApiKnowledge.cpp
        src/CompletionTypeResolver.h src/CompletionTypeResolver.cpp)
    target_include_directories(WFScriptEditorKnowledgeSmoke PRIVATE src)
    target_link_libraries(WFScriptEditorKnowledgeSmoke PRIVATE Qt6::Widgets)
    add_test(NAME WFScriptEditorKnowledgeSmoke COMMAND WFScriptEditorKnowledgeSmoke)
    set_tests_properties(WFScriptEditorKnowledgeSmoke PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()

# v5.6.6 Guided Development smoke: catalog-backed explorer, typo detection,
# visible error navigation and editor diagnostics. No hardware or SDK DLL.
if(WFSCRIPT_BUILD_TESTS)
    qt_add_executable(WFScriptGuidedDevelopmentSmoke
        tests/guided_development_smoke.cpp
        src/ApiExplorer.h src/ApiExplorer.cpp
        src/ApiDiagnostics.h src/ApiDiagnostics.cpp
        src/ScriptEditor.h src/ScriptEditor.cpp
        src/ModuleExportCatalog.h src/ModuleExportCatalog.cpp
        src/EditorAssist.h src/EditorAssist.cpp
        src/JavaScriptHighlighter.h src/JavaScriptHighlighter.cpp
        src/CompletionCatalog.h src/CompletionCatalog.cpp
        src/ApiKnowledge.h src/ApiKnowledge.cpp
        src/CompletionTypeResolver.h src/CompletionTypeResolver.cpp)
    target_include_directories(WFScriptGuidedDevelopmentSmoke PRIVATE src)
    target_link_libraries(WFScriptGuidedDevelopmentSmoke PRIVATE Qt6::Widgets)
    add_test(NAME WFScriptGuidedDevelopmentSmoke COMMAND WFScriptGuidedDevelopmentSmoke)
    set_tests_properties(WFScriptGuidedDevelopmentSmoke PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()

# Panel ownership/lifecycle regression. It instantiates the real MainWindow and
# PanelDesigner without launching a script or requesting DWF hardware.
if(WFSCRIPT_BUILD_TESTS)
    get_target_property(_wfscript_main_sources WFScript SOURCES)
    list(REMOVE_ITEM _wfscript_main_sources src/main.cpp)
    qt_add_executable(WFScriptPanelLifecycleSmoke
        tests/panel_script_lifecycle_smoke.cpp
        ${_wfscript_main_sources})
    target_include_directories(WFScriptPanelLifecycleSmoke PRIVATE src)
    target_link_libraries(WFScriptPanelLifecycleSmoke PRIVATE Qt6::Widgets Qt6::Qml Qt6::Sql Qt6::Network Qt6::SerialPort)
    add_test(NAME WFScriptPanelLifecycleSmoke COMMAND WFScriptPanelLifecycleSmoke)
    set_tests_properties(WFScriptPanelLifecycleSmoke PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()

# v5.6.9: stable in-tab API Explorer and Panel Designer visibility smoke test.
if(WFSCRIPT_BUILD_TESTS)
    get_target_property(_wfs569_sources WFScript SOURCES)
    list(REMOVE_ITEM _wfs569_sources src/main.cpp)
    qt_add_executable(WFScriptStableWorkspaceSmoke
        tests/stable_workspace_smoke.cpp
        ${_wfs569_sources})
    target_include_directories(WFScriptStableWorkspaceSmoke PRIVATE src)
    target_link_libraries(WFScriptStableWorkspaceSmoke PRIVATE Qt6::Widgets Qt6::Qml Qt6::Sql Qt6::Network Qt6::SerialPort)
    add_test(NAME WFScriptStableWorkspaceSmoke COMMAND WFScriptStableWorkspaceSmoke)
    set_tests_properties(WFScriptStableWorkspaceSmoke PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()

# v5.6.11: fixed-size canvas equals document bounds; oversize scrollbars,
# shrink/grow, saved dimensions and focused designer layout tests. No script engine or AD3 DLL required.
if(WFSCRIPT_BUILD_TESTS)
    qt_add_executable(WFScriptPanelSizingSmoke
        src/resources/icons.qrc
        tests/panel_sizing_smoke.cpp
        src/PanelDesigner.h src/PanelDesigner.cpp
        src/panel/PageAppearanceEditor.h src/panel/PageAppearanceEditor.cpp
        src/PanelCanvas.h src/PanelCanvas.cpp
        ${WFSCRIPT_WIDGET_SOURCES}
        src/WaveformPlot.h src/WaveformPlot.cpp
        src/ScopeFrame.h)
    target_include_directories(WFScriptPanelSizingSmoke PRIVATE src)
    target_link_libraries(WFScriptPanelSizingSmoke PRIVATE Qt6::Widgets)
    add_test(NAME WFScriptPanelSizingSmoke COMMAND WFScriptPanelSizingSmoke)
    set_tests_properties(WFScriptPanelSizingSmoke PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()

# v5.6.12 graphical designer: multi-select, alignment, undo/redo and clipboard.
# No Digilent SDK, device, scripts or Qt Test module required.
if(WFSCRIPT_BUILD_TESTS)
    qt_add_executable(WFScriptPanelDesignerActionsSmoke
        tests/panel_designer_actions_smoke.cpp
        src/PanelCanvas.h src/PanelCanvas.cpp
        ${WFSCRIPT_WIDGET_SOURCES}
        src/WaveformPlot.h src/WaveformPlot.cpp
        src/ScopeFrame.h)
    target_include_directories(WFScriptPanelDesignerActionsSmoke PRIVATE src)
    target_link_libraries(WFScriptPanelDesignerActionsSmoke PRIVATE Qt6::Widgets)
    add_test(NAME WFScriptPanelDesignerActionsSmoke COMMAND WFScriptPanelDesignerActionsSmoke)
    set_tests_properties(WFScriptPanelDesignerActionsSmoke PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()

# RC1.5.5: automated edge/centre alignment and non-interactive guide overlay.
if(WFSCRIPT_BUILD_TESTS)
    qt_add_executable(WFScriptPanelSmartGuidesSmoke
        tests/panel_smart_guides_smoke.cpp
        src/panel/PanelSmartGuides.h
        src/PanelCanvas.h src/PanelCanvas.cpp
        ${WFSCRIPT_WIDGET_SOURCES}
        src/WaveformPlot.h src/WaveformPlot.cpp
        src/ScopeFrame.h)
    target_include_directories(WFScriptPanelSmartGuidesSmoke PRIVATE src)
    target_link_libraries(WFScriptPanelSmartGuidesSmoke PRIVATE Qt6::Widgets)
    add_test(NAME WFScriptPanelSmartGuidesSmoke COMMAND WFScriptPanelSmartGuidesSmoke)
    set_tests_properties(WFScriptPanelSmartGuidesSmoke PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()

# v5.6.13 graphical designer: marquee selection and auto-applying properties.
if(WFSCRIPT_BUILD_TESTS)
    qt_add_executable(WFScriptPanelInteractionSmoke
        src/resources/icons.qrc
        tests/panel_interaction_smoke.cpp
        src/PanelDesigner.h src/PanelDesigner.cpp
        src/panel/PageAppearanceEditor.h src/panel/PageAppearanceEditor.cpp
        src/PanelCanvas.h src/PanelCanvas.cpp
        ${WFSCRIPT_WIDGET_SOURCES}
        src/WaveformPlot.h src/WaveformPlot.cpp
        src/ScopeFrame.h)
    target_include_directories(WFScriptPanelInteractionSmoke PRIVATE src)
    target_link_libraries(WFScriptPanelInteractionSmoke PRIVATE Qt6::Widgets)
    add_test(NAME WFScriptPanelInteractionSmoke COMMAND WFScriptPanelInteractionSmoke)
    set_tests_properties(WFScriptPanelInteractionSmoke PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()

# v5.6.14 reusable libraries. Uses a real QJSEngine but no AD3 device or DWF SDK.
if(WFSCRIPT_BUILD_TESTS)
    qt_add_executable(WFScriptLibrariesSmoke
        tests/script_library_smoke.cpp
        src/ScriptLibraryBridge.h src/ScriptLibraryBridge.cpp)
    target_include_directories(WFScriptLibrariesSmoke PRIVATE src)
    target_link_libraries(WFScriptLibrariesSmoke PRIVATE Qt6::Core Qt6::Qml)
    add_test(NAME WFScriptLibrariesSmoke COMMAND WFScriptLibrariesSmoke)
endif()

# v0.2-dev.8.6: self-contained DSP Studio third-party modules and expanded library limit.
if(WFSCRIPT_BUILD_TESTS)
    qt_add_executable(WFScriptDSPModulesSmoke
        tests/dsp_modules_smoke.cpp
        src/resources/examples.qrc
        src/PanelProject.h src/PanelProject.cpp
        src/ScriptLibraryBridge.h src/ScriptLibraryBridge.cpp)
    target_include_directories(WFScriptDSPModulesSmoke PRIVATE src)
    target_link_libraries(WFScriptDSPModulesSmoke PRIVATE Qt6::Core Qt6::Qml)
    add_test(NAME WFScriptDSPModulesSmoke COMMAND WFScriptDSPModulesSmoke)
endif()

# v0.2-dev.8.9: guided interactive AC Network Laboratory (no AD3 required).
if(WFSCRIPT_BUILD_TESTS)
    qt_add_executable(WFScriptACNetworkLabSmoke
        tests/ac_network_lab_smoke.cpp
        src/resources/examples.qrc
        src/PanelProject.h src/PanelProject.cpp
        src/ScriptLibraryBridge.h src/ScriptLibraryBridge.cpp)
    target_include_directories(WFScriptACNetworkLabSmoke PRIVATE src)
    target_link_libraries(WFScriptACNetworkLabSmoke PRIVATE Qt6::Core Qt6::Qml)
    add_test(NAME WFScriptACNetworkLabSmoke COMMAND WFScriptACNetworkLabSmoke)
endif()

# v0.2-dev.8.9.3.2: QJSEngine runtime regression for embedded RC Step Laboratory.
if(WFSCRIPT_BUILD_TESTS)
    qt_add_executable(WFScriptRCStepLabSmoke
        tests/rc_step_lab_smoke.cpp
        src/resources/examples.qrc
        src/PanelProject.h src/PanelProject.cpp
        src/ScriptLibraryBridge.h src/ScriptLibraryBridge.cpp)
    target_include_directories(WFScriptRCStepLabSmoke PRIVATE src)
    target_link_libraries(WFScriptRCStepLabSmoke PRIVATE Qt6::Core Qt6::Qml)
    add_test(NAME WFScriptRCStepLabSmoke COMMAND WFScriptRCStepLabSmoke)
endif()

# v5.6.15 structured JavaScript diagnostics. Requires no DWF hardware.
if(WFSCRIPT_BUILD_TESTS)
    qt_add_executable(WFScriptScriptLocationSmoke
        tests/script_location_smoke.cpp
        src/ScriptErrorDetails.h src/ScriptErrorDetails.cpp
        src/ScriptLibraryBridge.h src/ScriptLibraryBridge.cpp)
    target_include_directories(WFScriptScriptLocationSmoke PRIVATE src)
    target_link_libraries(WFScriptScriptLocationSmoke PRIVATE Qt6::Core Qt6::Qml)
    add_test(NAME WFScriptScriptLocationSmoke COMMAND WFScriptScriptLocationSmoke)
endif()

# v5.6.16: real QJSEngine integration for named classic includes, preserving
# global declarations, embedded/file sources, delayed callbacks and errors.
if(WFSCRIPT_BUILD_TESTS)
    qt_add_executable(WFScriptClassicIncludeDiagnosticsSmoke
        tests/classic_include_diagnostics_smoke.cpp
        src/ScriptLibraryBridge.h src/ScriptLibraryBridge.cpp
        src/ScriptErrorDetails.h src/ScriptErrorDetails.cpp)
    target_include_directories(WFScriptClassicIncludeDiagnosticsSmoke PRIVATE src)
    target_link_libraries(WFScriptClassicIncludeDiagnosticsSmoke PRIVATE Qt6::Core Qt6::Qml)
    add_test(NAME WFScriptClassicIncludeDiagnosticsSmoke
             COMMAND WFScriptClassicIncludeDiagnosticsSmoke)
endif()

# v5.6.17 editor-only native-export index and embedded stack display tests.
# No hardware, no Qt Widgets, no GUI required.
if(WFSCRIPT_BUILD_TESTS)
    qt_add_executable(WFScriptModuleAssistanceSmoke
        tests/module_export_catalog_smoke.cpp
        src/ModuleExportCatalog.h src/ModuleExportCatalog.cpp
        src/CompletionCatalog.h src/CompletionCatalog.cpp
        src/ScriptLibraryBridge.h src/ScriptLibraryBridge.cpp)
    target_include_directories(WFScriptModuleAssistanceSmoke PRIVATE src)
    target_link_libraries(WFScriptModuleAssistanceSmoke PRIVATE Qt6::Core Qt6::Qml)
    add_test(NAME WFScriptModuleAssistanceSmoke COMMAND WFScriptModuleAssistanceSmoke)
endif()

# GUI-level completion test for user modules, offscreen-capable. No hardware.
if(WFSCRIPT_BUILD_TESTS)
    qt_add_executable(WFScriptModuleEditorSmoke
        tests/module_editor_popup_smoke.cpp
        src/ScriptEditor.h src/ScriptEditor.cpp
        src/ModuleExportCatalog.h src/ModuleExportCatalog.cpp
        src/EditorAssist.h src/EditorAssist.cpp
        src/JavaScriptHighlighter.h src/JavaScriptHighlighter.cpp
        src/CompletionCatalog.h src/CompletionCatalog.cpp
        src/CompletionTypeResolver.h src/CompletionTypeResolver.cpp
        src/ApiKnowledge.h src/ApiKnowledge.cpp
        src/ApiDiagnostics.h src/ApiDiagnostics.cpp)
    target_include_directories(WFScriptModuleEditorSmoke PRIVATE src)
    target_link_libraries(WFScriptModuleEditorSmoke PRIVATE Qt6::Widgets)
    add_test(NAME WFScriptModuleEditorSmoke COMMAND WFScriptModuleEditorSmoke)
    set_tests_properties(WFScriptModuleEditorSmoke PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()

# v5.6.18: F1 can resolve a completed module expression without an open popup.
if(WFSCRIPT_BUILD_TESTS)
    qt_add_executable(WFScriptModuleHelpSmoke
        tests/module_help_resolution_smoke.cpp
        src/ModuleExportCatalog.h src/ModuleExportCatalog.cpp
        src/CompletionCatalog.h src/CompletionCatalog.cpp)
    target_include_directories(WFScriptModuleHelpSmoke PRIVATE src)
    target_link_libraries(WFScriptModuleHelpSmoke PRIVATE Qt6::Core)
    add_test(NAME WFScriptModuleHelpSmoke COMMAND WFScriptModuleHelpSmoke)

    qt_add_executable(WFScriptModuleF1EditorSmoke
        tests/module_f1_editor_smoke.cpp
        src/ScriptEditor.h src/ScriptEditor.cpp
        src/ModuleExportCatalog.h src/ModuleExportCatalog.cpp
        src/EditorAssist.h src/EditorAssist.cpp
        src/JavaScriptHighlighter.h src/JavaScriptHighlighter.cpp
        src/CompletionCatalog.h src/CompletionCatalog.cpp
        src/CompletionTypeResolver.h src/CompletionTypeResolver.cpp
        src/ApiKnowledge.h src/ApiKnowledge.cpp
        src/ApiDiagnostics.h src/ApiDiagnostics.cpp)
    target_include_directories(WFScriptModuleF1EditorSmoke PRIVATE src)
    target_link_libraries(WFScriptModuleF1EditorSmoke PRIVATE Qt6::Widgets)
    add_test(NAME WFScriptModuleF1EditorSmoke COMMAND WFScriptModuleF1EditorSmoke)
    set_tests_properties(WFScriptModuleF1EditorSmoke PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()

# v5.6.20 - exercise the actual PanelRuntime in Simulation, including
# interrupting infinite JS during startup and a button callback. No AD3 needed.
if(WFSCRIPT_BUILD_TESTS)
    qt_add_executable(WFScriptPanelRuntimeLifecycleSmoke
        tests/panel_runtime_lifecycle_smoke.cpp
        src/PanelRuntime.h src/PanelRuntime.cpp
        src/PowerSuppliesBridge.h src/PowerSuppliesBridge.cpp
        src/plot/PlotCommand.h src/plot/PlotBridge.h src/plot/PlotBridge.cpp
        src/ScriptLibraryBridge.h src/ScriptLibraryBridge.cpp
        src/ScriptErrorDetails.h src/ScriptErrorDetails.cpp
        src/ScriptWorker.h
        src/DwfRuntime.h src/DwfRuntime.cpp src/DwfProtocols.cpp
        src/WaveFormsBindings.h src/WaveFormsBindings.cpp
        src/ScopeMath.h src/ScopeMath.cpp
        src/ScopeAnalysis.h src/ScopeAnalysis.cpp
        src/ScopeFrame.h src/ScopeTrigger.h src/ScriptHelpers.h)
    target_sources(WFScriptPanelRuntimeLifecycleSmoke PRIVATE ${WFSCRIPT_PANEL_RUNTIME_SMOKE_DEPENDENCIES})
    target_include_directories(WFScriptPanelRuntimeLifecycleSmoke PRIVATE src)
    target_link_libraries(WFScriptPanelRuntimeLifecycleSmoke PRIVATE Qt6::Core Qt6::Qml Qt6::Sql Qt6::Network Qt6::SerialPort)
    add_test(NAME WFScriptPanelRuntimeLifecycleSmoke COMMAND WFScriptPanelRuntimeLifecycleSmoke)
    set_tests_properties(WFScriptPanelRuntimeLifecycleSmoke PROPERTIES TIMEOUT 18)
endif()

# v5.6.21: UI-independent panel project model, legacy v1 conversion,
# embedded libraries and lifecycle settings. No Qt Widgets/AD3 required.
if(WFSCRIPT_BUILD_TESTS)
    qt_add_executable(WFScriptPanelProjectSmoke
        tests/panel_project_smoke.cpp
        src/resources/examples.qrc
        src/PanelProject.h src/PanelProject.cpp)
    target_include_directories(WFScriptPanelProjectSmoke PRIVATE src)
    target_link_libraries(WFScriptPanelProjectSmoke PRIVATE Qt6::Core)
    add_test(NAME WFScriptPanelProjectSmoke COMMAND WFScriptPanelProjectSmoke)
endif()

# v0.2-dev.8: typed project-owned Panel Data, scalar bindings, worker bridge.
# No SDK connection, physical hardware, or native widgets required.
if(WFSCRIPT_BUILD_TESTS)
    qt_add_executable(WFScriptPanelDataSmoke
        tests/panel_data_smoke.cpp
        src/PanelProject.h src/PanelProject.cpp
        src/PanelUiBridge.h src/PanelDataSchema.h)
    target_include_directories(WFScriptPanelDataSmoke PRIVATE src)
    target_link_libraries(WFScriptPanelDataSmoke PRIVATE Qt6::Core Qt6::Qml)
    add_test(NAME WFScriptPanelDataSmoke COMMAND WFScriptPanelDataSmoke)
endif()

# v0.2-dev.8.4: run the production subscription JavaScript with a real
# QJSEngine and worker-side PanelUiBridge (does not require DWF hardware).
if(WFSCRIPT_BUILD_TESTS)
    qt_add_executable(WFScriptPanelDataEventsSmoke
        tests/panel_data_events_smoke.cpp
        src/PanelDataEvents.h src/PanelUiBridge.h src/PanelDataSchema.h)
    target_include_directories(WFScriptPanelDataEventsSmoke PRIVATE src)
    target_link_libraries(WFScriptPanelDataEventsSmoke PRIVATE Qt6::Core Qt6::Qml)
    add_test(NAME WFScriptPanelDataEventsSmoke COMMAND WFScriptPanelDataEventsSmoke)
endif()

# v0.2-dev.8.5: production computed dependency graph, typed native
# read-only variables and bound input/output integration, no hardware.
if(WFSCRIPT_BUILD_TESTS)
    qt_add_executable(WFScriptPanelDataComputedSmoke
        tests/panel_data_computed_smoke.cpp
        src/PanelDataEvents.h src/PanelUiBridge.h src/PanelDataSchema.h)
    target_include_directories(WFScriptPanelDataComputedSmoke PRIVATE src)
    target_link_libraries(WFScriptPanelDataComputedSmoke PRIVATE Qt6::Core Qt6::Qml)
    add_test(NAME WFScriptPanelDataComputedSmoke COMMAND WFScriptPanelDataComputedSmoke)
endif()

# v5.6.22: actual panel hook execution, errors and interrupted shutdown.
# No physical instruments, widgets or external JavaScript files are needed.
if(WFSCRIPT_BUILD_TESTS)
    qt_add_executable(WFScriptPanelHooksSmoke
        tests/panel_lifecycle_hooks_smoke.cpp
        src/PanelRuntime.h src/PanelRuntime.cpp
        src/PowerSuppliesBridge.h src/PowerSuppliesBridge.cpp
        src/plot/PlotCommand.h src/plot/PlotBridge.h src/plot/PlotBridge.cpp
        src/PanelProject.h
        src/ScriptLibraryBridge.h src/ScriptLibraryBridge.cpp
        src/ScriptErrorDetails.h src/ScriptErrorDetails.cpp
        src/ScriptWorker.h
        src/DwfRuntime.h src/DwfRuntime.cpp src/DwfProtocols.cpp
        src/WaveFormsBindings.h src/WaveFormsBindings.cpp
        src/ScopeMath.h src/ScopeMath.cpp
        src/ScopeAnalysis.h src/ScopeAnalysis.cpp
        src/ScopeFrame.h src/ScopeTrigger.h src/ScriptHelpers.h)
    target_sources(WFScriptPanelHooksSmoke PRIVATE ${WFSCRIPT_PANEL_RUNTIME_SMOKE_DEPENDENCIES})
    target_include_directories(WFScriptPanelHooksSmoke PRIVATE src)
    target_link_libraries(WFScriptPanelHooksSmoke PRIVATE Qt6::Core Qt6::Qml Qt6::Sql Qt6::Network Qt6::SerialPort)
    add_test(NAME WFScriptPanelHooksSmoke COMMAND WFScriptPanelHooksSmoke)
    set_tests_properties(WFScriptPanelHooksSmoke PROPERTIES TIMEOUT 30)
endif()

# v5.6.22: real Panel Settings dialog and Create/Go to Function buttons.
if(WFSCRIPT_BUILD_TESTS)
    get_target_property(_wfscript_5622_sources WFScript SOURCES)
    list(REMOVE_ITEM _wfscript_5622_sources src/main.cpp)
    qt_add_executable(WFScriptPanelSettingsSmoke
        tests/panel_lifecycle_settings_smoke.cpp
        ${_wfscript_5622_sources})
    target_include_directories(WFScriptPanelSettingsSmoke PRIVATE src)
    target_link_libraries(WFScriptPanelSettingsSmoke PRIVATE Qt6::Widgets Qt6::Qml Qt6::Sql Qt6::Network Qt6::SerialPort)
    add_test(NAME WFScriptPanelSettingsSmoke COMMAND WFScriptPanelSettingsSmoke)
    set_tests_properties(WFScriptPanelSettingsSmoke PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()

# v5.6.23: the Script Editor owns New/Open/Save/Run/Stop; the global
# toolbar only configures devices. The same native GUI test also checks
# Panel toolbar icons and stable top-level tab geometry.
if(WFSCRIPT_BUILD_TESTS)
    get_target_property(_wfs5623_sources WFScript SOURCES)
    list(REMOVE_ITEM _wfs5623_sources src/main.cpp)
    qt_add_executable(WFScriptWorkspaceToolbarSmoke
        tests/workspace_toolbar_smoke.cpp
        ${_wfs5623_sources})
    target_include_directories(WFScriptWorkspaceToolbarSmoke PRIVATE src)
    target_link_libraries(WFScriptWorkspaceToolbarSmoke PRIVATE Qt6::Widgets Qt6::Qml Qt6::Sql Qt6::Network Qt6::SerialPort)
    add_test(NAME WFScriptWorkspaceToolbarSmoke COMMAND WFScriptWorkspaceToolbarSmoke)
    set_tests_properties(WFScriptWorkspaceToolbarSmoke PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()

# v0.2-dev.8.9.6: run a simulation-only Panel through the real MainWindow,
# enter/exit the operator presentation, retain console logs and preserve all
# Page coordinates. Can run on Qt offscreen (no AD3 required).
if(WFSCRIPT_BUILD_TESTS)
    get_target_property(_wfs896_sources WFScript SOURCES)
    list(REMOVE_ITEM _wfs896_sources src/main.cpp)
    qt_add_executable(WFScriptOperatorViewSmoke
        tests/panel_operator_view_smoke.cpp
        ${_wfs896_sources})
    target_include_directories(WFScriptOperatorViewSmoke PRIVATE src)
    target_link_libraries(WFScriptOperatorViewSmoke PRIVATE Qt6::Widgets Qt6::Qml Qt6::Sql Qt6::Network Qt6::SerialPort)
    add_test(NAME WFScriptOperatorViewSmoke COMMAND WFScriptOperatorViewSmoke)
    set_tests_properties(WFScriptOperatorViewSmoke PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 30)
endif()

# v5.6.24: event inspector persistence and real source-aware event dispatch.
if(WFSCRIPT_BUILD_TESTS)
    qt_add_executable(WFScriptPanelEventInspectorSmoke
        src/resources/icons.qrc
        src/resources/examples.qrc
        tests/panel_event_binding_smoke.cpp
        src/PanelDesigner.h src/PanelDesigner.cpp
        src/panel/PageAppearanceEditor.h src/panel/PageAppearanceEditor.cpp
        src/PanelCanvas.h src/PanelCanvas.cpp
        ${WFSCRIPT_WIDGET_SOURCES}
        src/PanelProject.h src/PanelProject.cpp
        src/WaveformPlot.h src/WaveformPlot.cpp
        src/ScopeFrame.h src/PanelEventCatalog.h)
    target_include_directories(WFScriptPanelEventInspectorSmoke PRIVATE src)
    target_link_libraries(WFScriptPanelEventInspectorSmoke PRIVATE Qt6::Widgets)
    add_test(NAME WFScriptPanelEventInspectorSmoke COMMAND WFScriptPanelEventInspectorSmoke)
    set_tests_properties(WFScriptPanelEventInspectorSmoke PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

    qt_add_executable(WFScriptPanelEventRuntimeSmoke
        tests/panel_event_runtime_smoke.cpp
        src/PanelRuntime.h src/PanelRuntime.cpp
        src/PowerSuppliesBridge.h src/PowerSuppliesBridge.cpp
        src/plot/PlotCommand.h src/plot/PlotBridge.h src/plot/PlotBridge.cpp
        src/PanelProject.h src/PanelEventCatalog.h
        src/ScriptLibraryBridge.h src/ScriptLibraryBridge.cpp
        src/ScriptErrorDetails.h src/ScriptErrorDetails.cpp
        src/ScriptWorker.h
        src/DwfRuntime.h src/DwfRuntime.cpp src/DwfProtocols.cpp
        src/WaveFormsBindings.h src/WaveFormsBindings.cpp
        src/ScopeMath.h src/ScopeMath.cpp
        src/ScopeAnalysis.h src/ScopeAnalysis.cpp
        src/ScopeFrame.h src/ScopeTrigger.h src/ScriptHelpers.h)
    target_sources(WFScriptPanelEventRuntimeSmoke PRIVATE ${WFSCRIPT_PANEL_RUNTIME_SMOKE_DEPENDENCIES})
    target_include_directories(WFScriptPanelEventRuntimeSmoke PRIVATE src)
    target_link_libraries(WFScriptPanelEventRuntimeSmoke PRIVATE Qt6::Core Qt6::Qml Qt6::Sql Qt6::Network Qt6::SerialPort)
    add_test(NAME WFScriptPanelEventRuntimeSmoke COMMAND WFScriptPanelEventRuntimeSmoke)
    set_tests_properties(WFScriptPanelEventRuntimeSmoke PROPERTIES TIMEOUT 16)
endif()

# v5.6.25 native Page model, designer tab switching, and schema v2 round trip.
if(WFSCRIPT_BUILD_TESTS)
    qt_add_executable(WFScriptMultiPageSmoke
        src/resources/icons.qrc
        tests/multi_page_smoke.cpp
        src/PanelDesigner.h src/PanelDesigner.cpp
        src/panel/PageAppearanceEditor.h src/panel/PageAppearanceEditor.cpp
        src/PanelCanvas.h src/PanelCanvas.cpp
        ${WFSCRIPT_WIDGET_SOURCES}
        src/PanelProject.h src/PanelProject.cpp
        src/WaveformPlot.h src/WaveformPlot.cpp
        src/ScopeFrame.h src/PanelEventCatalog.h)
    target_include_directories(WFScriptMultiPageSmoke PRIVATE src)
    target_link_libraries(WFScriptMultiPageSmoke PRIVATE Qt6::Widgets)
    add_test(NAME WFScriptMultiPageSmoke COMMAND WFScriptMultiPageSmoke)
    set_tests_properties(WFScriptMultiPageSmoke PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()

# v5.6.29: widget factory contract / serialized schema / persistent Pages.
if(WFSCRIPT_BUILD_TESTS)
    qt_add_executable(WFScriptWidgetRegistrySmoke
        tests/widget_registry_smoke.cpp
        src/PanelCanvas.h src/PanelCanvas.cpp
        ${WFSCRIPT_WIDGET_SOURCES}
        src/WaveformPlot.h src/WaveformPlot.cpp
        src/ScopeFrame.h)
    target_include_directories(WFScriptWidgetRegistrySmoke PRIVATE src)
    target_link_libraries(WFScriptWidgetRegistrySmoke PRIVATE Qt6::Widgets)
    add_test(NAME WFScriptWidgetRegistrySmoke COMMAND WFScriptWidgetRegistrySmoke)
    set_tests_properties(WFScriptWidgetRegistrySmoke PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()

# v0.2-dev.8.3: offscreen regression for all widget factories, preserved
# native Button/Label typography, and Numeric/Toggle/Gauge paint/resize.
if(WFSCRIPT_BUILD_TESTS)
    qt_add_executable(WFScriptBasicWidgetVisualSmoke
        tests/basic_widget_visual_smoke.cpp
        src/PanelCanvas.h src/PanelCanvas.cpp
        ${WFSCRIPT_WIDGET_SOURCES}
        src/WaveformPlot.h src/WaveformPlot.cpp
        src/ScopeFrame.h)
    target_include_directories(WFScriptBasicWidgetVisualSmoke PRIVATE src)
    target_link_libraries(WFScriptBasicWidgetVisualSmoke PRIVATE Qt6::Widgets)
    add_test(NAME WFScriptBasicWidgetVisualSmoke COMMAND WFScriptBasicWidgetVisualSmoke)
    set_tests_properties(WFScriptBasicWidgetVisualSmoke PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()

# v5.6.30: metadata-driven property form and typed extended .wfpanel fields.
if(WFSCRIPT_BUILD_TESTS)
    qt_add_executable(WFScriptWidgetPropertiesSmoke
        tests/widget_properties_smoke.cpp
        src/PanelCanvas.h src/PanelCanvas.cpp
        ${WFSCRIPT_WIDGET_SOURCES}
        src/WaveformPlot.h src/WaveformPlot.cpp
        src/ScopeFrame.h)
    target_include_directories(WFScriptWidgetPropertiesSmoke PRIVATE src)
    target_link_libraries(WFScriptWidgetPropertiesSmoke PRIVATE Qt6::Widgets)
    add_test(NAME WFScriptWidgetPropertiesSmoke COMMAND WFScriptWidgetPropertiesSmoke)
    set_tests_properties(WFScriptWidgetPropertiesSmoke PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()

# v5.6.31: new native Digital Display widget (offscreen, no AD3 required).
if(WFSCRIPT_BUILD_TESTS)
    qt_add_executable(WFScriptDigitalDisplaySmoke
        tests/digital_display_smoke.cpp
        src/PanelCanvas.h src/PanelCanvas.cpp
        ${WFSCRIPT_WIDGET_SOURCES}
        src/WaveformPlot.h src/WaveformPlot.cpp
        src/ScopeFrame.h)
    target_include_directories(WFScriptDigitalDisplaySmoke PRIVATE src)
    target_link_libraries(WFScriptDigitalDisplaySmoke PRIVATE Qt6::Widgets)
    add_test(NAME WFScriptDigitalDisplaySmoke COMMAND WFScriptDigitalDisplaySmoke)
    set_tests_properties(WFScriptDigitalDisplaySmoke PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()

# v5.6.32: polished native measurement widgets; offscreen, no AD3 required.
if(WFSCRIPT_BUILD_TESTS)
    qt_add_executable(WFScriptProfessionalWidgetsSmoke
        tests/professional_widgets_smoke.cpp
        src/PanelCanvas.h src/PanelCanvas.cpp
        ${WFSCRIPT_WIDGET_SOURCES}
        src/WaveformPlot.h src/WaveformPlot.cpp
        src/ScopeFrame.h)
    target_include_directories(WFScriptProfessionalWidgetsSmoke PRIVATE src)
    target_link_libraries(WFScriptProfessionalWidgetsSmoke PRIVATE Qt6::Widgets)
    add_test(NAME WFScriptProfessionalWidgetsSmoke COMMAND WFScriptProfessionalWidgetsSmoke)
    set_tests_properties(WFScriptProfessionalWidgetsSmoke PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()

# v5.6.33: professional native input controls and multi-state status display.
if(WFSCRIPT_BUILD_TESTS)
    qt_add_executable(WFScriptProfessionalInputsSmoke
        tests/professional_inputs_smoke.cpp
        src/PanelCanvas.h src/PanelCanvas.cpp
        ${WFSCRIPT_WIDGET_SOURCES}
        src/WaveformPlot.h src/WaveformPlot.cpp
        src/ScopeFrame.h)
    target_include_directories(WFScriptProfessionalInputsSmoke PRIVATE src)
    target_link_libraries(WFScriptProfessionalInputsSmoke PRIVATE Qt6::Widgets)
    add_test(NAME WFScriptProfessionalInputsSmoke COMMAND WFScriptProfessionalInputsSmoke)
    set_tests_properties(WFScriptProfessionalInputsSmoke PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()

# v5.6.33.2: touchpad wheel accumulation, true circular dragging,
# and stable/responsive spin-box typography. No AD3 required.
if(WFSCRIPT_BUILD_TESTS)
    qt_add_executable(WFScriptInputInteractionSmoke
        tests/professional_input_interaction_smoke.cpp
        src/widgets/RotaryInputWidget.h src/widgets/RotaryInputWidget.cpp
        src/widgets/NumericStepperWidget.h src/widgets/NumericStepperWidget.cpp
        src/widgets/InstrumentVisuals.h)
    target_include_directories(WFScriptInputInteractionSmoke PRIVATE src)
    target_link_libraries(WFScriptInputInteractionSmoke PRIVATE Qt6::Widgets)
    add_test(NAME WFScriptInputInteractionSmoke COMMAND WFScriptInputInteractionSmoke)
    set_tests_properties(WFScriptInputInteractionSmoke PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()

# RC1.3.1: native labeled text/IPv4 inputs, string binding, visual events and
# strict IP validation; offscreen test needs no AD3 or network instrument.
if(WFSCRIPT_BUILD_TESTS)
    qt_add_executable(WFScriptTextInputSmoke
        tests/text_input_widget_smoke.cpp
        src/PanelUiBridge.h
        src/PanelCanvas.h src/PanelCanvas.cpp
        ${WFSCRIPT_WIDGET_SOURCES}
        src/WaveformPlot.h src/WaveformPlot.cpp
        src/ScopeFrame.h)
    target_include_directories(WFScriptTextInputSmoke PRIVATE src)
    target_link_libraries(WFScriptTextInputSmoke PRIVATE Qt6::Widgets Qt6::Qml)
    add_test(NAME WFScriptTextInputSmoke COMMAND WFScriptTextInputSmoke)
    set_tests_properties(WFScriptTextInputSmoke PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()

# v5.6.34: theme/project JSON, inspector and native widget lifetime smoke.
if(WFSCRIPT_BUILD_TESTS)
    qt_add_executable(WFScriptPanelThemeSmoke
        src/resources/icons.qrc
        tests/panel_theme_smoke.cpp
        src/PanelProject.h src/PanelProject.cpp
        src/PanelDesigner.h src/PanelDesigner.cpp
        src/panel/PanelAppearance.h
        src/panel/PageAppearanceEditor.h src/panel/PageAppearanceEditor.cpp
        src/PanelCanvas.h src/PanelCanvas.cpp
        ${WFSCRIPT_WIDGET_SOURCES}
        src/WaveformPlot.h src/WaveformPlot.cpp
        src/ScopeFrame.h src/PanelEventCatalog.h)
    target_include_directories(WFScriptPanelThemeSmoke PRIVATE src)
    target_link_libraries(WFScriptPanelThemeSmoke PRIVATE Qt6::Widgets)
    add_test(NAME WFScriptPanelThemeSmoke COMMAND WFScriptPanelThemeSmoke)
    set_tests_properties(WFScriptPanelThemeSmoke PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()

# v5.6.35: Panel-only plot rendering and JSON property contract; no hardware.
if(WFSCRIPT_BUILD_TESTS)
    qt_add_executable(WFScriptIndependentPlotsSmoke
        tests/independent_plots_smoke.cpp
        src/plot/PlotCommand.h
        src/plot/PanelChartWidget.h src/plot/PanelChartWidget.cpp
        src/plot/PanelPlotFactory.h src/plot/PanelPlotFactory.cpp
    src/plot/DigitalTimingPlotFactory.h src/plot/DigitalTimingPlotFactory.cpp)
    target_include_directories(WFScriptIndependentPlotsSmoke PRIVATE src)
    target_link_libraries(WFScriptIndependentPlotsSmoke PRIVATE Qt6::Widgets)
    add_test(NAME WFScriptIndependentPlotsSmoke COMMAND WFScriptIndependentPlotsSmoke)
    set_tests_properties(WFScriptIndependentPlotsSmoke PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()

# v5.6.35.1: image-based offscreen regression for line-path painter state.
if(WFSCRIPT_BUILD_TESTS)
    qt_add_executable(WFScriptChartStrokeRenderSmoke
        tests/chart_stroke_render_smoke.cpp
        src/plot/PlotCommand.h
        src/plot/PanelChartWidget.h src/plot/PanelChartWidget.cpp)
    target_include_directories(WFScriptChartStrokeRenderSmoke PRIVATE src)
    target_link_libraries(WFScriptChartStrokeRenderSmoke PRIVATE Qt6::Widgets)
    add_test(NAME WFScriptChartStrokeRenderSmoke COMMAND WFScriptChartStrokeRenderSmoke)
    set_tests_properties(WFScriptChartStrokeRenderSmoke PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()

# v5.6.36: independent series style + log axes and cursor regression (offscreen)
if(WFSCRIPT_BUILD_TESTS)
    qt_add_executable(WFScriptChartMeasurementSmoke
        tests/chart_measurement_smoke.cpp
        src/plot/PlotCommand.h
        src/plot/PanelChartWidget.h src/plot/PanelChartWidget.cpp)
    target_include_directories(WFScriptChartMeasurementSmoke PRIVATE src)
    target_link_libraries(WFScriptChartMeasurementSmoke PRIVATE Qt6::Widgets)
    add_test(NAME WFScriptChartMeasurementSmoke COMMAND WFScriptChartMeasurementSmoke)
    set_tests_properties(WFScriptChartMeasurementSmoke PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()

# v5.6.36.2: manual axis and series stroke inheritance regression test
if(WFSCRIPT_BUILD_TESTS)
    add_executable(WFScriptChartAxisStrokeReviewSmoke
        tests/chart_axis_stroke_review_smoke.cpp
        src/plot/PanelChartWidget.h src/plot/PanelChartWidget.cpp)
    target_include_directories(WFScriptChartAxisStrokeReviewSmoke PRIVATE src)
    target_link_libraries(WFScriptChartAxisStrokeReviewSmoke PRIVATE Qt6::Widgets)
    add_test(NAME WFScriptChartAxisStrokeReviewSmoke COMMAND WFScriptChartAxisStrokeReviewSmoke)
    set_tests_properties(WFScriptChartAxisStrokeReviewSmoke PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()

# v5.6.37: new registered digital timing renderer and dual-axis chart checks.
if(WFSCRIPT_BUILD_TESTS)
    qt_add_executable(WFScriptEngineeringChartsSmoke
        tests/chart_engineering_smoke.cpp
        src/plot/PlotCommand.h
        src/plot/PanelChartWidget.h src/plot/PanelChartWidget.cpp
        src/plot/PanelPlotFactory.h src/plot/PanelPlotFactory.cpp
        src/plot/DigitalTimingPlotFactory.h src/plot/DigitalTimingPlotFactory.cpp)
    target_include_directories(WFScriptEngineeringChartsSmoke PRIVATE src)
    target_link_libraries(WFScriptEngineeringChartsSmoke PRIVATE Qt6::Widgets)
    add_test(NAME WFScriptEngineeringChartsSmoke COMMAND WFScriptEngineeringChartsSmoke)
    set_tests_properties(WFScriptEngineeringChartsSmoke PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()

# v5.6.38: data-bounded mouse inspection and digital edge metrics
if(WFSCRIPT_BUILD_TESTS)
    qt_add_executable(WFScriptChartInspectionSmoke
        tests/chart_inspection_smoke.cpp
        src/plot/PanelChartWidget.h src/plot/PanelChartWidget.cpp)
    target_include_directories(WFScriptChartInspectionSmoke PRIVATE src)
    target_link_libraries(WFScriptChartInspectionSmoke PRIVATE Qt6::Widgets)
    add_test(NAME WFScriptChartInspectionSmoke COMMAND WFScriptChartInspectionSmoke)
    set_tests_properties(WFScriptChartInspectionSmoke PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()

# v5.6.38.1: modifier-gated, sample-snapped measurement cursors
if(WFSCRIPT_BUILD_TESTS)
    qt_add_executable(WFScriptCursorSnapSmoke
        tests/chart_cursor_snap_smoke.cpp
        src/plot/PanelChartWidget.h src/plot/PanelChartWidget.cpp)
    target_include_directories(WFScriptCursorSnapSmoke PRIVATE src)
    target_link_libraries(WFScriptCursorSnapSmoke PRIVATE Qt6::Widgets)
    add_test(NAME WFScriptCursorSnapSmoke COMMAND WFScriptCursorSnapSmoke)
    set_tests_properties(WFScriptCursorSnapSmoke PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()

# v5.6.41: Panel/Page Property inspector consolidation (no AD3 required).
if(WFSCRIPT_BUILD_TESTS)
    qt_add_executable(WFScriptPanelPropertiesSmoke
        src/resources/icons.qrc
        tests/panel_consolidated_properties_smoke.cpp
        src/PanelDesigner.h src/PanelDesigner.cpp
        src/panel/PageAppearanceEditor.h src/panel/PageAppearanceEditor.cpp
        src/PanelCanvas.h src/PanelCanvas.cpp
        src/PanelProject.h src/PanelProject.cpp
        ${WFSCRIPT_WIDGET_SOURCES}
        src/WaveformPlot.h src/WaveformPlot.cpp
        src/ScopeFrame.h)
    target_include_directories(WFScriptPanelPropertiesSmoke PRIVATE src)
    target_link_libraries(WFScriptPanelPropertiesSmoke PRIVATE Qt6::Widgets)
    add_test(NAME WFScriptPanelPropertiesSmoke COMMAND WFScriptPanelPropertiesSmoke)
    set_tests_properties(WFScriptPanelPropertiesSmoke PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()

# v5.6.44 — actual QJSEngine/C++ Power Supplies integration test; no hardware or DWF DLL.
if(WFSCRIPT_BUILD_TESTS)
    qt_add_executable(WFScriptSuppliesSmoke
        tests/supplies_api_smoke.cpp
        src/PowerSuppliesBridge.h src/PowerSuppliesBridge.cpp
        src/DwfRuntime.h src/DwfRuntime.cpp src/DwfProtocols.cpp
        src/CompletionCatalog.h src/CompletionCatalog.cpp
        src/ApiKnowledge.h src/ApiKnowledge.cpp)
    target_include_directories(WFScriptSuppliesSmoke PRIVATE src)
    target_link_libraries(WFScriptSuppliesSmoke PRIVATE Qt6::Core Qt6::Qml)
    add_test(NAME WFScriptSuppliesSmoke COMMAND WFScriptSuppliesSmoke)
endif()

# v5.6.52 — Label multiline Property Inspector and Qt HTML subset rendering.
# Tests run offscreen and do not require a Digilent device or WaveForms SDK DLL.
if(WFSCRIPT_BUILD_TESTS)
    qt_add_executable(WFScriptLabelFormattingSmoke
        tests/label_text_format_smoke.cpp
        src/resources/examples.qrc
        src/PanelCanvas.h src/PanelCanvas.cpp
        ${WFSCRIPT_WIDGET_SOURCES}
        src/WaveformPlot.h src/WaveformPlot.cpp
        src/ScopeFrame.h)
    target_include_directories(WFScriptLabelFormattingSmoke PRIVATE src)
    target_link_libraries(WFScriptLabelFormattingSmoke PRIVATE Qt6::Widgets)
    add_test(NAME WFScriptLabelFormattingSmoke COMMAND WFScriptLabelFormattingSmoke)
    set_tests_properties(WFScriptLabelFormattingSmoke PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()

# v5.6.53 — Measurement Result + Measurement Table factory and Qt rendering smoke.
# Offscreen, no AD3 / DWF SDK required. Uses the existing widget registry/codec.
if(WFSCRIPT_BUILD_TESTS)
    qt_add_executable(WFScriptMeasurementWidgetsSmoke
        tests/measurement_widgets_smoke.cpp
        src/PanelCanvas.h src/PanelCanvas.cpp
        ${WFSCRIPT_WIDGET_SOURCES}
        src/WaveformPlot.h src/WaveformPlot.cpp
        src/ScopeFrame.h)
    target_include_directories(WFScriptMeasurementWidgetsSmoke PRIVATE src)
    target_link_libraries(WFScriptMeasurementWidgetsSmoke PRIVATE Qt6::Widgets Qt6::Qml)
    add_test(NAME WFScriptMeasurementWidgetsSmoke COMMAND WFScriptMeasurementWidgetsSmoke)
    set_tests_properties(WFScriptMeasurementWidgetsSmoke PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()


# v5.6.55: standalone native GUI Event Log validation (no AD3 / DWF DLL).
if(WFSCRIPT_BUILD_TESTS)
    qt_add_executable(WFScriptEventLogSmoke
        tests/event_log_smoke.cpp
        src/widgets/EventLogWidget.h src/widgets/EventLogWidget.cpp
        src/widgets/EventLogWidgetFactory.h src/widgets/EventLogWidgetFactory.cpp)
    target_include_directories(WFScriptEventLogSmoke PRIVATE src)
    target_link_libraries(WFScriptEventLogSmoke PRIVATE Qt6::Widgets Qt6::Qml)
    add_test(NAME WFScriptEventLogSmoke COMMAND WFScriptEventLogSmoke)
    set_tests_properties(WFScriptEventLogSmoke PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()

# RC1.5.14.4: crossing Event Log retention capacity must stay incremental.
if(WFSCRIPT_BUILD_TESTS)
    qt_add_executable(WFScriptEventLogCapacityRC15144Smoke
        tests/event_log_capacity_rc15144_smoke.cpp
        src/widgets/EventLogWidget.h src/widgets/EventLogWidget.cpp)
    target_include_directories(WFScriptEventLogCapacityRC15144Smoke PRIVATE src)
    target_link_libraries(WFScriptEventLogCapacityRC15144Smoke PRIVATE Qt6::Widgets Qt6::Qml)
    add_test(NAME WFScriptEventLogCapacityRC15144Smoke COMMAND WFScriptEventLogCapacityRC15144Smoke)
    set_tests_properties(WFScriptEventLogCapacityRC15144Smoke PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()

# v5.6.56 — actual QJSEngine-to-C++-to-JS test runner smoke, no DWF DLL.
if(WFSCRIPT_BUILD_TESTS)
    qt_add_executable(WFScriptTestRunnerSmoke
        tests/test_runner_cpp_smoke.cpp
        src/TestRunnerBridge.h src/TestRunnerBridge.cpp
        src/TestResultsRepository.cpp)
    target_include_directories(WFScriptTestRunnerSmoke PRIVATE src)
    target_link_libraries(WFScriptTestRunnerSmoke PRIVATE Qt6::Core Qt6::Qml)
    add_test(NAME WFScriptTestRunnerSmoke COMMAND WFScriptTestRunnerSmoke)
endif()

# v5.6.57 — actual Qt timer, deadline, native completion and imported .mjs tests.
if(WFSCRIPT_BUILD_TESTS)
    qt_add_executable(WFScriptTimedTestRunnerSmoke
        tests/test_runner_timed_smoke.cpp
        src/TestRunnerBridge.h src/TestRunnerBridge.cpp
        src/TestResultsRepository.cpp
        src/ScriptLibraryBridge.h src/ScriptLibraryBridge.cpp)
    target_include_directories(WFScriptTimedTestRunnerSmoke PRIVATE src)
    target_link_libraries(WFScriptTimedTestRunnerSmoke PRIVATE Qt6::Core Qt6::Qml)
    add_test(NAME WFScriptTimedTestRunnerSmoke COMMAND WFScriptTimedTestRunnerSmoke)
endif()


# v5.6.58 -- real QJSEngine lifecycle and result-record verification.
if(WFSCRIPT_BUILD_TESTS)
    qt_add_executable(WFScriptLifecycleRunnerSmoke
        tests/test_runner_lifecycle_smoke.cpp
        src/TestRunnerBridge.h src/TestRunnerBridge.cpp
        src/TestResultsRepository.cpp)
    target_include_directories(WFScriptLifecycleRunnerSmoke PRIVATE src)
    target_link_libraries(WFScriptLifecycleRunnerSmoke PRIVATE Qt6::Core Qt6::Qml)
    add_test(NAME WFScriptLifecycleRunnerSmoke COMMAND WFScriptLifecycleRunnerSmoke)
endif()

# v5.6.59 — real QJSEngine native UUT context and run-data lifecycle checks.
if(WFSCRIPT_BUILD_TESTS)
    qt_add_executable(WFScriptRunContextSmoke
        tests/test_runner_context_smoke.cpp
        src/TestRunnerBridge.h src/TestRunnerBridge.cpp
        src/TestResultsRepository.cpp)
    target_include_directories(WFScriptRunContextSmoke PRIVATE src)
    target_link_libraries(WFScriptRunContextSmoke PRIVATE Qt6::Core Qt6::Qml)
    add_test(NAME WFScriptRunContextSmoke COMMAND WFScriptRunContextSmoke)
endif()

# v5.6.60: actual offscreen Instrument & Script API Explorer tree, including
# guide-only nodes; and native Qt/C++ file-export verification.
if(WFSCRIPT_BUILD_TESTS)
    qt_add_executable(WFScriptTestExecutiveExplorerSmoke
        tests/test_api_explorer_560_smoke.cpp
        src/ApiExplorer.h src/ApiExplorer.cpp
        src/ApiKnowledge.h src/ApiKnowledge.cpp
        src/CompletionCatalog.h src/CompletionCatalog.cpp)
    target_include_directories(WFScriptTestExecutiveExplorerSmoke PRIVATE src)
    target_link_libraries(WFScriptTestExecutiveExplorerSmoke PRIVATE Qt6::Widgets)
    add_test(NAME WFScriptTestExecutiveExplorerSmoke COMMAND WFScriptTestExecutiveExplorerSmoke)
    set_tests_properties(WFScriptTestExecutiveExplorerSmoke PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    qt_add_executable(WFScriptReportExportSmoke
        tests/test_runner_report_export_smoke.cpp
        src/TestRunnerBridge.h src/TestRunnerBridge.cpp
        src/TestResultsRepository.cpp)
    target_include_directories(WFScriptReportExportSmoke PRIVATE src)
    target_link_libraries(WFScriptReportExportSmoke PRIVATE Qt6::Core Qt6::Qml)
    add_test(NAME WFScriptReportExportSmoke COMMAND WFScriptReportExportSmoke)
endif()

# v5.6.61 -- declarative sequence validation and .wftest.json roundtrip.
if(WFSCRIPT_BUILD_TESTS)
    qt_add_executable(WFScriptDefinitionRunnerSmoke
        tests/test_runner_definition_smoke.cpp
        src/TestRunnerBridge.h src/TestRunnerBridge.cpp
        src/TestResultsRepository.cpp)
    target_include_directories(WFScriptDefinitionRunnerSmoke PRIVATE src)
    target_link_libraries(WFScriptDefinitionRunnerSmoke PRIVATE Qt6::Core Qt6::Qml)
    add_test(NAME WFScriptDefinitionRunnerSmoke COMMAND WFScriptDefinitionRunnerSmoke)
endif()

# Shared rich JavaScript editor used by the main workspace AND Test Sequence.
# The same ScriptEditor (completion, F1 help, diagnostics, line numbers) must
# be linked into every standalone native TestSequenceEditor GUI CTest.
set(WFSCRIPT_TEST_EDITOR_SCRIPT_SOURCES
    src/ScriptEditor.h src/ScriptEditor.cpp
    src/EditorAssist.h src/EditorAssist.cpp
    src/JavaScriptHighlighter.h src/JavaScriptHighlighter.cpp
    src/ApiDiagnostics.h src/ApiDiagnostics.cpp
    src/CompletionCatalog.h src/CompletionCatalog.cpp
    src/CompletionTypeResolver.h src/CompletionTypeResolver.cpp
    src/ModuleExportCatalog.h src/ModuleExportCatalog.cpp
    src/ApiKnowledge.h src/ApiKnowledge.cpp)

# v5.6.62: offline visual editor and strict embedded JSON-literal roundtrip.
# Does not require an AD3, QJSEngine or real hardware; offscreen Qt Widgets.
if(WFSCRIPT_BUILD_TESTS)
    qt_add_executable(WFScriptVisualTestSequenceSmoke
        tests/test_sequence_editor_562_smoke.cpp
        src/TestSequenceEditor.h src/TestSequenceEditor.cpp
        src/RunInformationEditor.h src/RunInformationEditor.cpp
        ${WFSCRIPT_TEST_EDITOR_SCRIPT_SOURCES})
    target_include_directories(WFScriptVisualTestSequenceSmoke PRIVATE src)
    target_link_libraries(WFScriptVisualTestSequenceSmoke PRIVATE Qt6::Widgets)
    add_test(NAME WFScriptVisualTestSequenceSmoke COMMAND WFScriptVisualTestSequenceSmoke)
    set_tests_properties(WFScriptVisualTestSequenceSmoke PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()

# RC1.5.7: readable Test Sequence Editor (offscreen Qt Widgets). Verifies
# procedure vs completion-poll presentation, editing, grouping, JSON preview,
# and the unchanged definition schema used by the native Test Executive.
if(WFSCRIPT_BUILD_TESTS)
    qt_add_executable(WFScriptTestSequenceEditorUXSmoke
        tests/test_sequence_editor_rc157_smoke.cpp
        src/TestSequenceEditor.h src/TestSequenceEditor.cpp
        src/RunInformationEditor.h src/RunInformationEditor.cpp
        ${WFSCRIPT_TEST_EDITOR_SCRIPT_SOURCES})
    target_include_directories(WFScriptTestSequenceEditorUXSmoke PRIVATE src)
    target_link_libraries(WFScriptTestSequenceEditorUXSmoke PRIVATE Qt6::Widgets)
    add_test(NAME WFScriptTestSequenceEditorUXSmoke COMMAND WFScriptTestSequenceEditorUXSmoke)
    set_tests_properties(WFScriptTestSequenceEditorUXSmoke PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()

# v5.6.63: native project configuration guard and C++ numeric limits.
if(WFSCRIPT_BUILD_TESTS)
    qt_add_executable(WFScriptProjectExecutiveSmoke
        tests/test_project_native_executive_smoke.cpp
        src/TestRunnerBridge.h src/TestRunnerBridge.cpp
        src/TestResultsRepository.cpp)
    target_include_directories(WFScriptProjectExecutiveSmoke PRIVATE src)
    target_link_libraries(WFScriptProjectExecutiveSmoke PRIVATE Qt6::Core Qt6::Qml)
    add_test(NAME WFScriptProjectExecutiveSmoke COMMAND WFScriptProjectExecutiveSmoke)
endif()

# v5.6.63: project-owned test definition survives save / reopen unchanged.
if(WFSCRIPT_BUILD_TESTS)
    qt_add_executable(WFScriptProjectDefinitionRoundtrip
        tests/test_project_definition_roundtrip.cpp
        src/PanelProject.h src/PanelProject.cpp)
    target_include_directories(WFScriptProjectDefinitionRoundtrip PRIVATE src)
    target_link_libraries(WFScriptProjectDefinitionRoundtrip PRIVATE Qt6::Core)
    add_test(NAME WFScriptProjectDefinitionRoundtrip COMMAND WFScriptProjectDefinitionRoundtrip)
endif()

# v5.6.64: native UUT form plus pre-execution validation, immutable run metadata.
if(WFSCRIPT_BUILD_TESTS)
    qt_add_executable(WFScriptRunInformationSmoke
        tests/run_information_smoke.cpp
        src/TestRunnerBridge.h src/TestRunnerBridge.cpp
        src/TestResultsRepository.cpp
        src/RunInformationSchema.h)
    target_include_directories(WFScriptRunInformationSmoke PRIVATE src)
    target_link_libraries(WFScriptRunInformationSmoke PRIVATE Qt6::Core Qt6::Qml)
    add_test(NAME WFScriptRunInformationSmoke COMMAND WFScriptRunInformationSmoke)

    qt_add_executable(WFScriptRunInformationWidgetSmoke
        tests/run_information_widget_smoke.cpp
        src/widgets/RunInformationWidget.h src/widgets/RunInformationWidget.cpp
        src/RunInformationSchema.h)
    target_include_directories(WFScriptRunInformationWidgetSmoke PRIVATE src)
    target_link_libraries(WFScriptRunInformationWidgetSmoke PRIVATE Qt6::Widgets)
    add_test(NAME WFScriptRunInformationWidgetSmoke COMMAND WFScriptRunInformationWidgetSmoke)
    set_tests_properties(WFScriptRunInformationWidgetSmoke PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()

# v5.6.66 — automatic JSON archive: success/failure, immutability, error handling,
# UUT, execution UUID, mode metadata and no startup side effects.
if(WFSCRIPT_BUILD_TESTS)
    qt_add_executable(WFScriptArchiveSmoke
        tests/test_report_archiving_566_smoke.cpp
        src/TestRunnerBridge.h src/TestRunnerBridge.cpp
        src/TestResultsRepository.h src/TestResultsRepository.cpp)
    target_include_directories(WFScriptArchiveSmoke PRIVATE src)
    target_link_libraries(WFScriptArchiveSmoke PRIVATE Qt6::Core Qt6::Qml)
    add_test(NAME WFScriptArchiveSmoke COMMAND WFScriptArchiveSmoke)
endif()

# v5.6.67 — archived history reader/export: native headless regression.
if(WFSCRIPT_BUILD_TESTS)
    qt_add_executable(WFScriptResultsHistorySmoke
        tests/test_results_history_567_smoke.cpp
        src/TestResultsRepository.h src/TestResultsHistory.cpp)
    target_include_directories(WFScriptResultsHistorySmoke PRIVATE src)
    target_link_libraries(WFScriptResultsHistorySmoke PRIVATE Qt6::Core)
    add_test(NAME WFScriptResultsHistorySmoke COMMAND WFScriptResultsHistorySmoke)

    qt_add_executable(WFScriptResultsBrowserSmoke
        tests/test_results_browser_567_smoke.cpp
        src/TestResultsRepository.h src/TestResultsHistory.cpp
        src/TestResultsBrowserDialog.h src/TestResultsBrowserDialog.cpp
        src/TestResultsTableModels.h src/TestResultsTableModels.cpp)
    target_include_directories(WFScriptResultsBrowserSmoke PRIVATE src)
    target_link_libraries(WFScriptResultsBrowserSmoke PRIVATE Qt6::Widgets)
    add_test(NAME WFScriptResultsBrowserSmoke COMMAND WFScriptResultsBrowserSmoke)
    set_tests_properties(WFScriptResultsBrowserSmoke PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()

# v5.6.69.1 educational image/card controls and regression coverage. Offscreen, no WaveForms DLL or AD3.
if(WFSCRIPT_BUILD_TESTS)
    qt_add_executable(WFScriptEducationWidgetsSmoke
        tests/education_widgets_smoke.cpp
        src/PanelCanvas.h src/PanelCanvas.cpp
        ${WFSCRIPT_WIDGET_SOURCES}
        src/WaveformPlot.h src/WaveformPlot.cpp src/ScopeFrame.h)
    target_include_directories(WFScriptEducationWidgetsSmoke PRIVATE src)
    target_link_libraries(WFScriptEducationWidgetsSmoke PRIVATE Qt6::Widgets)
    add_test(NAME WFScriptEducationWidgetsSmoke COMMAND WFScriptEducationWidgetsSmoke)
    set_tests_properties(WFScriptEducationWidgetsSmoke PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()

# v5.6.70 experiment widgets: offscreen Qt smoke, independent of AD3/DWF DLL.
if(WFSCRIPT_BUILD_TESTS)
    qt_add_executable(WFScriptExperimentWidgetsSmoke
        tests/experiment_widgets_smoke.cpp
        src/PanelCanvas.h src/PanelCanvas.cpp
        ${WFSCRIPT_WIDGET_SOURCES}
        src/WaveformPlot.h src/WaveformPlot.cpp src/ScopeFrame.h)
    target_include_directories(WFScriptExperimentWidgetsSmoke PRIVATE src)
    target_link_libraries(WFScriptExperimentWidgetsSmoke PRIVATE Qt6::Widgets Qt6::Qml)
    add_test(NAME WFScriptExperimentWidgetsSmoke COMMAND WFScriptExperimentWidgetsSmoke)
    set_tests_properties(WFScriptExperimentWidgetsSmoke PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()

# v5.6.70.2 — actual QJSEngine native-value contract (headless, no Digilent DLL).
if(WFSCRIPT_BUILD_TESTS)
    qt_add_executable(WFScriptNativeJSValuesSmoke
        tests/panel_javascript_values_smoke.cpp
        src/PanelUiBridge.h src/ScriptValueConversion.h
        src/TestRunnerBridge.h src/TestRunnerBridge.cpp
        src/TestResultsRepository.h src/TestResultsRepository.cpp)
    target_include_directories(WFScriptNativeJSValuesSmoke PRIVATE src)
    target_link_libraries(WFScriptNativeJSValuesSmoke PRIVATE Qt6::Core Qt6::Qml)
    add_test(NAME WFScriptNativeJSValuesSmoke COMMAND WFScriptNativeJSValuesSmoke)
endif()

# Standard install target. Windows Qt runtime DLLs must be deployed separately
# with windeployqt (see docs/BUILD_WINDOWS.md).
include(GNUInstallDirs)
install(TARGETS WFScript RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} BUNDLE DESTINATION .)

# SQLite worker + real QJSEngine callback / round-trip regression (no AD3).
if(WFSCRIPT_BUILD_TESTS)
    qt_add_executable(WFScriptSQLiteSmoke
        tests/SqliteSmoke.cpp
        src/SqliteBridge.h src/SqliteBridge.cpp
        src/ScriptValueConversion.h)
    target_include_directories(WFScriptSQLiteSmoke PRIVATE src)
    target_link_libraries(WFScriptSQLiteSmoke PRIVATE Qt6::Core Qt6::Qml Qt6::Sql)
    add_test(NAME WFScriptSQLiteSmoke COMMAND WFScriptSQLiteSmoke)
    set_tests_properties(WFScriptSQLiteSmoke PROPERTIES TIMEOUT 20)
endif()

# v0.2-dev.6: localhost serial/TCP/UDP API, binary bytes and JS callbacks.
# No external hardware or DWF SDK required; real Windows COM tests are manual.
if(WFSCRIPT_BUILD_TESTS)
    qt_add_executable(WFScriptCommunicationsSmoke
        tests/communications_smoke.cpp
        src/CommunicationBridge.h src/CommunicationBridge.cpp
        src/BinaryBridge.h src/BinaryBridge.cpp
        src/ScriptValueConversion.h)
    target_include_directories(WFScriptCommunicationsSmoke PRIVATE src)
    target_link_libraries(WFScriptCommunicationsSmoke PRIVATE
        Qt6::Core Qt6::Qml Qt6::Network Qt6::SerialPort)
    add_test(NAME WFScriptCommunicationsSmoke COMMAND WFScriptCommunicationsSmoke)
    set_tests_properties(WFScriptCommunicationsSmoke PROPERTIES TIMEOUT 12)
endif()

# v0.2-dev.7: transport-independent Communications Terminal widget, offscreen.
# Uses the real Qt factory, sendRequested EventSink and ui.set() value contract.
if(WFSCRIPT_BUILD_TESTS)
    qt_add_executable(WFScriptCommunicationsTerminalSmoke
        tests/communications_terminal_smoke.cpp
        src/widgets/CommunicationsTerminalWidget.h src/widgets/CommunicationsTerminalWidget.cpp
        src/widgets/CommunicationsTerminalWidgetFactory.h src/widgets/CommunicationsTerminalWidgetFactory.cpp)
    target_include_directories(WFScriptCommunicationsTerminalSmoke PRIVATE src)
    target_link_libraries(WFScriptCommunicationsTerminalSmoke PRIVATE Qt6::Widgets)
    add_test(NAME WFScriptCommunicationsTerminalSmoke COMMAND WFScriptCommunicationsTerminalSmoke)
    set_tests_properties(WFScriptCommunicationsTerminalSmoke PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()

# v0.2-dev.8.9.8: tabbed Diagnostics Console + live Panel Data Watch;
# filtered per-run logs, wrapping, value highlighting and user layout settings.
# Test still uses a real PanelDataLab simulation with no physical hardware.
if(WFSCRIPT_BUILD_TESTS)
    get_target_property(_wfs897_sources WFScript SOURCES)
    list(REMOVE_ITEM _wfs897_sources src/main.cpp)
    qt_add_executable(WFScriptOperatorDockWatchSmoke
        tests/panel_operator_dock_watch_smoke.cpp
        ${_wfs897_sources})
    target_include_directories(WFScriptOperatorDockWatchSmoke PRIVATE src)
    target_link_libraries(WFScriptOperatorDockWatchSmoke PRIVATE
        Qt6::Widgets Qt6::Qml Qt6::Sql Qt6::Network Qt6::SerialPort)
    add_test(NAME WFScriptOperatorDockWatchSmoke COMMAND WFScriptOperatorDockWatchSmoke)
    set_tests_properties(WFScriptOperatorDockWatchSmoke PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 35)
endif()

# v0.2 candidate Binary Lab: native buffer, streaming IEEE frame, and real TCP echo.
if(WFSCRIPT_BUILD_TESTS)
    qt_add_executable(WFScriptBinaryBridgeSmoke
        tests/binary_bridge_smoke.cpp
        src/BinaryBridge.h src/BinaryBridge.cpp
        src/CommunicationBridge.h src/CommunicationBridge.cpp
        src/ScriptValueConversion.h)
    target_include_directories(WFScriptBinaryBridgeSmoke PRIVATE src)
    target_link_libraries(WFScriptBinaryBridgeSmoke PRIVATE
        Qt6::Core Qt6::Qml Qt6::Network Qt6::SerialPort)
    add_test(NAME WFScriptBinaryBridgeSmoke COMMAND WFScriptBinaryBridgeSmoke)
    set_tests_properties(WFScriptBinaryBridgeSmoke PROPERTIES TIMEOUT 15)
endif()

# RC1.5 - native chart dense-envelope and cache performance correction. Pure
# index/envelope unit tests require no GUI; QWidget offscreen checks geometry budgets.
if(WFSCRIPT_BUILD_TESTS)
    add_executable(WFScriptNativeEnvelopeStress tests/native_envelope_stress.cpp)
    add_test(NAME WFScriptNativeEnvelopeStress COMMAND WFScriptNativeEnvelopeStress)
    add_executable(WFScriptNativeSampleIndexSmoke tests/native_sample_index_smoke.cpp)
    add_test(NAME WFScriptNativeSampleIndexSmoke COMMAND WFScriptNativeSampleIndexSmoke)
    qt_add_executable(WFScriptNativeChartSmoke
        tests/native_chart_smoke.cpp
        src/plot/PlotCommand.h
        src/plot/NativeSampleIndex.h
        src/plot/PanelChartWidget.h src/plot/PanelChartWidget.cpp)
    target_include_directories(WFScriptNativeChartSmoke PRIVATE src)
    target_link_libraries(WFScriptNativeChartSmoke PRIVATE Qt6::Widgets)
    add_test(NAME WFScriptNativeChartSmoke COMMAND WFScriptNativeChartSmoke)
    set_tests_properties(WFScriptNativeChartSmoke PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()

# RC1.5.2 legacy XY performance: a Qt-independent irregular-X index test and
# an optional offscreen QWidget smoke. Performance thresholds are measured in
# native paint primitives, not wall-clock benchmarks on different computers.
if(WFSCRIPT_BUILD_TESTS)
    add_executable(WFScriptLegacyXYIndexStress tests/legacy_xy_index_stress.cpp)
    add_test(NAME WFScriptLegacyXYIndexStress COMMAND WFScriptLegacyXYIndexStress)
    qt_add_executable(WFScriptLegacyChartPerformanceSmoke
        tests/legacy_chart_performance_smoke.cpp
        src/plot/PlotCommand.h src/plot/NativeSampleIndex.h
        src/plot/LegacyXYIndex.h
        src/plot/PanelChartWidget.h src/plot/PanelChartWidget.cpp)
    target_include_directories(WFScriptLegacyChartPerformanceSmoke PRIVATE src)
    target_link_libraries(WFScriptLegacyChartPerformanceSmoke PRIVATE Qt6::Widgets)
    add_test(NAME WFScriptLegacyChartPerformanceSmoke COMMAND WFScriptLegacyChartPerformanceSmoke)
    set_tests_properties(WFScriptLegacyChartPerformanceSmoke PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 35)
endif()

# RC1.5.3 shared pixel-grid + edge-indexed Digital Timing/Protocol plotting.
# The two index tests require only a C++17 compiler; the QWidget test is
# conditional on the optional Qt CTest build. No hardware required.
if(WFSCRIPT_BUILD_TESTS)
    add_executable(WFScriptPixelColumnAlignmentStress
        tests/chart_pixel_alignment_stress.cpp)
    add_test(NAME WFScriptPixelColumnAlignmentStress COMMAND WFScriptPixelColumnAlignmentStress)
    add_executable(WFScriptDigitalEdgeIndexStress tests/digital_edge_index_stress.cpp)
    add_test(NAME WFScriptDigitalEdgeIndexStress COMMAND WFScriptDigitalEdgeIndexStress)
    qt_add_executable(WFScriptDigitalChartPerformanceSmoke
        tests/digital_chart_performance_smoke.cpp
        src/plot/PlotCommand.h src/plot/PlotPixelColumns.h
        src/plot/NativeSampleIndex.h src/plot/LegacyXYIndex.h
        src/plot/DigitalEdgeIndex.h
        src/plot/PanelChartWidget.h src/plot/PanelChartWidget.cpp)
    target_include_directories(WFScriptDigitalChartPerformanceSmoke PRIVATE src)
    target_link_libraries(WFScriptDigitalChartPerformanceSmoke PRIVATE Qt6::Widgets)
    add_test(NAME WFScriptDigitalChartPerformanceSmoke COMMAND WFScriptDigitalChartPerformanceSmoke)
    set_tests_properties(WFScriptDigitalChartPerformanceSmoke PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 40)
endif()

# RC1.5.4.1: cross-chart linear X tick precision regression (QtCore only).
if(WFSCRIPT_BUILD_TESTS)
    qt_add_executable(WFScriptChartAxisLabelsSmoke
        tests/chart_axis_labels_smoke.cpp src/plot/PlotAxisLabels.h)
    target_include_directories(WFScriptChartAxisLabelsSmoke PRIVATE src)
    target_link_libraries(WFScriptChartAxisLabelsSmoke PRIVATE Qt6::Core)
    add_test(NAME WFScriptChartAxisLabelsSmoke COMMAND WFScriptChartAxisLabelsSmoke)
endif()

# RC1.5.6 — retained test-results widget hierarchy; independent of AD3 or Rigol.
if(WFSCRIPT_BUILD_TESTS)
    qt_add_executable(WFScriptResultWidgetsIncrementalSmoke
        tests/test_result_widgets_incremental_smoke.cpp
        src/widgets/TestSequenceWidget.h src/widgets/TestSequenceWidget.cpp
        src/widgets/MeasurementTableWidget.h src/widgets/MeasurementTableWidget.cpp
        src/widgets/EventLogWidget.h src/widgets/EventLogWidget.cpp)
    target_include_directories(WFScriptResultWidgetsIncrementalSmoke PRIVATE src)
    target_link_libraries(WFScriptResultWidgetsIncrementalSmoke PRIVATE Qt6::Widgets)
    add_test(NAME WFScriptResultWidgetsIncrementalSmoke COMMAND WFScriptResultWidgetsIncrementalSmoke)
    set_tests_properties(WFScriptResultWidgetsIncrementalSmoke PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()

# RC1.5.8: Modeless, integrated test authoring editor (offscreen native Qt).
if(WFSCRIPT_BUILD_TESTS)
    qt_add_executable(WFScriptTestSequenceEditorRC158Smoke
        tests/test_sequence_editor_rc158_smoke.cpp
        src/TestSequenceEditor.h src/TestSequenceEditor.cpp
        src/RunInformationEditor.h src/RunInformationEditor.cpp
        ${WFSCRIPT_TEST_EDITOR_SCRIPT_SOURCES})
    target_include_directories(WFScriptTestSequenceEditorRC158Smoke PRIVATE src)
    target_link_libraries(WFScriptTestSequenceEditorRC158Smoke PRIVATE Qt6::Widgets)
    add_test(NAME WFScriptTestSequenceEditorRC158Smoke COMMAND WFScriptTestSequenceEditorRC158Smoke)
    set_tests_properties(WFScriptTestSequenceEditorRC158Smoke PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()

# RC1.5.9: real QJSEngine callback completion and parameter passing tests.
if(WFSCRIPT_BUILD_TESTS)
    qt_add_executable(WFScriptTestRunnerCallbackSmoke
        tests/test_runner_callback_smoke.cpp
        src/TestRunnerBridge.h src/TestRunnerBridge.cpp
        src/TestResultsRepository.cpp)
    target_include_directories(WFScriptTestRunnerCallbackSmoke PRIVATE src)
    target_link_libraries(WFScriptTestRunnerCallbackSmoke PRIVATE Qt6::Core Qt6::Qml)
    add_test(NAME WFScriptTestRunnerCallbackSmoke COMMAND WFScriptTestRunnerCallbackSmoke)
endif()

# RC1.5.9: embedded ScriptEditor + JSON parameters + callback mode.
if(WFSCRIPT_BUILD_TESTS)
    qt_add_executable(WFScriptTestSequenceEditorRC159Smoke
        tests/test_sequence_editor_rc159_smoke.cpp
        src/TestSequenceEditor.h src/TestSequenceEditor.cpp
        src/RunInformationEditor.h src/RunInformationEditor.cpp
        ${WFSCRIPT_TEST_EDITOR_SCRIPT_SOURCES})
    target_include_directories(WFScriptTestSequenceEditorRC159Smoke PRIVATE src)
    target_link_libraries(WFScriptTestSequenceEditorRC159Smoke PRIVATE Qt6::Widgets)
    add_test(NAME WFScriptTestSequenceEditorRC159Smoke COMMAND WFScriptTestSequenceEditorRC159Smoke)
    set_tests_properties(WFScriptTestSequenceEditorRC159Smoke PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()

# RC1.5.10: structured multi-measurement callbacks, arrays and atomic validation.
if(WFSCRIPT_BUILD_TESTS)
    qt_add_executable(WFScriptTestRunnerStructuredResultsSmoke
        tests/test_runner_structured_results_smoke.cpp
        src/TestRunnerBridge.h src/TestRunnerBridge.cpp
        src/StructuredTestResults.h src/TestResultsRepository.cpp)
    target_include_directories(WFScriptTestRunnerStructuredResultsSmoke PRIVATE src)
    target_link_libraries(WFScriptTestRunnerStructuredResultsSmoke PRIVATE Qt6::Core Qt6::Qml)
    add_test(NAME WFScriptTestRunnerStructuredResultsSmoke COMMAND WFScriptTestRunnerStructuredResultsSmoke)
endif()

# RC1.5.10: native named-result table and always-reachable editor actions.
if(WFSCRIPT_BUILD_TESTS)
    qt_add_executable(WFScriptTestSequenceEditorRC1510Smoke
        tests/test_sequence_editor_rc1510_smoke.cpp
        src/TestSequenceEditor.h src/TestSequenceEditor.cpp
        src/StructuredTestResults.h
        src/RunInformationEditor.h src/RunInformationEditor.cpp
        ${WFSCRIPT_TEST_EDITOR_SCRIPT_SOURCES})
    target_include_directories(WFScriptTestSequenceEditorRC1510Smoke PRIVATE src)
    target_link_libraries(WFScriptTestSequenceEditorRC1510Smoke PRIVATE Qt6::Widgets)
    add_test(NAME WFScriptTestSequenceEditorRC1510Smoke COMMAND WFScriptTestSequenceEditorRC1510Smoke)
    set_tests_properties(WFScriptTestSequenceEditorRC1510Smoke PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()

# RC1.5.14.2: expanded-step/measurement capacity and bounded live-window regression.
if(WFSCRIPT_BUILD_TESTS)
    qt_add_executable(WFScriptTestRunnerCapacityRC15142Smoke
        tests/test_runner_capacity_rc1514_2_smoke.cpp
        src/TestSequenceCapacity.h src/TestSequenceLoops.h
        src/TestRunnerBridge.h src/TestRunnerBridge.cpp
        src/StructuredTestResults.h src/TestResultsRepository.cpp)
    target_include_directories(WFScriptTestRunnerCapacityRC15142Smoke PRIVATE src)
    target_link_libraries(WFScriptTestRunnerCapacityRC15142Smoke PRIVATE Qt6::Core Qt6::Qml)
    add_test(NAME WFScriptTestRunnerCapacityRC15142Smoke COMMAND WFScriptTestRunnerCapacityRC15142Smoke)
endif()

# RC1.5.11: bounded declarative sweeps/loops, reference values and native execution.
if(WFSCRIPT_BUILD_TESTS)
    qt_add_executable(WFScriptTestSequenceLoopsSmoke
        tests/test_sequence_loops_smoke.cpp
        src/TestSequenceLoops.h
        src/TestRunnerBridge.h src/TestRunnerBridge.cpp
        src/StructuredTestResults.h src/TestResultsRepository.cpp)
    target_include_directories(WFScriptTestSequenceLoopsSmoke PRIVATE src)
    target_link_libraries(WFScriptTestSequenceLoopsSmoke PRIVATE Qt6::Core Qt6::Qml)
    add_test(NAME WFScriptTestSequenceLoopsSmoke COMMAND WFScriptTestSequenceLoopsSmoke)
endif()

if(WFSCRIPT_BUILD_TESTS)
    qt_add_executable(WFScriptTestSequenceEditorRC1511Smoke
        tests/test_sequence_editor_rc1511_smoke.cpp
        src/TestSequenceLoops.h
        src/TestSequenceEditor.h src/TestSequenceEditor.cpp
        src/StructuredTestResults.h
        src/RunInformationEditor.h src/RunInformationEditor.cpp
        ${WFSCRIPT_TEST_EDITOR_SCRIPT_SOURCES})
    target_include_directories(WFScriptTestSequenceEditorRC1511Smoke PRIVATE src)
    target_link_libraries(WFScriptTestSequenceEditorRC1511Smoke PRIVATE Qt6::Widgets)
    add_test(NAME WFScriptTestSequenceEditorRC1511Smoke COMMAND WFScriptTestSequenceEditorRC1511Smoke)
    set_tests_properties(WFScriptTestSequenceEditorRC1511Smoke PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()

# RC1.5.12: modeless hierarchical test editor, inline sweep editing, and
# lossless preservation of child relative/named-result acceptance rules.
if(WFSCRIPT_BUILD_TESTS)
    qt_add_executable(WFScriptTestSequenceEditorRC1512Smoke
        tests/test_sequence_editor_rc1512_smoke.cpp
        src/TestSequenceLoops.h
        src/TestSequenceEditor.h src/TestSequenceEditor.cpp
        src/StructuredTestResults.h
        src/RunInformationEditor.h src/RunInformationEditor.cpp
        ${WFSCRIPT_TEST_EDITOR_SCRIPT_SOURCES})
    target_include_directories(WFScriptTestSequenceEditorRC1512Smoke PRIVATE src)
    target_link_libraries(WFScriptTestSequenceEditorRC1512Smoke PRIVATE Qt6::Widgets)
    add_test(NAME WFScriptTestSequenceEditorRC1512Smoke COMMAND WFScriptTestSequenceEditorRC1512Smoke)
    set_tests_properties(WFScriptTestSequenceEditorRC1512Smoke PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()

# RC1.5.13: read-only human-readable execution preview uses the exact native
# expansion logic, supports filtered sweep iterations and preserves authored data.
if(WFSCRIPT_BUILD_TESTS)
    qt_add_executable(WFScriptTestSequenceEditorRC1513Smoke
        tests/test_sequence_editor_rc1513_smoke.cpp
        src/TestSequenceLoops.h
        src/TestSequenceEditor.h src/TestSequenceEditor.cpp
        src/StructuredTestResults.h
        src/RunInformationEditor.h src/RunInformationEditor.cpp
        ${WFSCRIPT_TEST_EDITOR_SCRIPT_SOURCES})
    target_include_directories(WFScriptTestSequenceEditorRC1513Smoke PRIVATE src)
    target_link_libraries(WFScriptTestSequenceEditorRC1513Smoke PRIVATE Qt6::Widgets)
    add_test(NAME WFScriptTestSequenceEditorRC1513Smoke COMMAND WFScriptTestSequenceEditorRC1513Smoke)
    set_tests_properties(WFScriptTestSequenceEditorRC1513Smoke PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()

# RC1.5.14: bounded native conditional branching and declared variables.
if(WFSCRIPT_BUILD_TESTS)
    qt_add_executable(WFScriptTestConditionalRC1514Smoke
        tests/test_conditional_rc1514_smoke.cpp
        src/TestSequenceConditions.h src/TestSequenceLoops.h
        src/TestRunnerBridge.h src/TestRunnerBridge.cpp
        src/StructuredTestResults.h src/TestResultsRepository.cpp)
    target_include_directories(WFScriptTestConditionalRC1514Smoke PRIVATE src)
    target_link_libraries(WFScriptTestConditionalRC1514Smoke PRIVATE Qt6::Core Qt6::Qml)
    add_test(NAME WFScriptTestConditionalRC1514Smoke COMMAND WFScriptTestConditionalRC1514Smoke)
endif()

if(WFSCRIPT_BUILD_TESTS)
    qt_add_executable(WFScriptTestSequenceEditorRC1514Smoke
        tests/test_sequence_editor_rc1514_smoke.cpp
        src/TestSequenceConditions.h src/TestSequenceLoops.h
        src/TestSequenceEditor.h src/TestSequenceEditor.cpp
        src/StructuredTestResults.h
        src/RunInformationEditor.h src/RunInformationEditor.cpp
        ${WFSCRIPT_TEST_EDITOR_SCRIPT_SOURCES})
    target_include_directories(WFScriptTestSequenceEditorRC1514Smoke PRIVATE src)
    target_link_libraries(WFScriptTestSequenceEditorRC1514Smoke PRIVATE Qt6::Widgets)
    add_test(NAME WFScriptTestSequenceEditorRC1514Smoke COMMAND WFScriptTestSequenceEditorRC1514Smoke)
    set_tests_properties(WFScriptTestSequenceEditorRC1514Smoke PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()

# RC1.5.14.1: Sweep/Repeat as direct children of IF/ELSE, including under an
# outer sweep. Verify authored tree order and native expansion preservation.
if(WFSCRIPT_BUILD_TESTS)
    qt_add_executable(WFScriptTestSequenceEditorNestedConditionalSmoke
        tests/test_sequence_editor_nested_conditionals_rc1514_1_smoke.cpp
        src/TestSequenceConditions.h src/TestSequenceLoops.h
        src/TestSequenceEditor.h src/TestSequenceEditor.cpp
        src/StructuredTestResults.h
        src/RunInformationEditor.h src/RunInformationEditor.cpp
        ${WFSCRIPT_TEST_EDITOR_SCRIPT_SOURCES})
    target_include_directories(WFScriptTestSequenceEditorNestedConditionalSmoke PRIVATE src)
    target_link_libraries(WFScriptTestSequenceEditorNestedConditionalSmoke PRIVATE Qt6::Widgets)
    add_test(NAME WFScriptTestSequenceEditorNestedConditionalSmoke COMMAND WFScriptTestSequenceEditorNestedConditionalSmoke)
    set_tests_properties(WFScriptTestSequenceEditorNestedConditionalSmoke PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()

# RC1.5.14.3 — genuine virtual GUI models, 10k rows, full-range scroll,
# and follow-tail behavior. No AD3 / Rigol hardware required.
if(WFSCRIPT_BUILD_TESTS)
    qt_add_executable(WFScriptLargeSweepVirtualRC15143Smoke
        tests/large_sweep_virtual_rc15143_smoke.cpp
        src/TestSequenceCapacity.h
        src/widgets/TestSequenceWidget.h src/widgets/TestSequenceWidget.cpp
        src/widgets/MeasurementTableWidget.h src/widgets/MeasurementTableWidget.cpp
        src/widgets/EventLogWidget.h src/widgets/EventLogWidget.cpp)
    target_include_directories(WFScriptLargeSweepVirtualRC15143Smoke PRIVATE src)
    target_link_libraries(WFScriptLargeSweepVirtualRC15143Smoke PRIVATE Qt6::Widgets)
    add_test(NAME WFScriptLargeSweepVirtualRC15143Smoke COMMAND WFScriptLargeSweepVirtualRC15143Smoke)
    set_tests_properties(WFScriptLargeSweepVirtualRC15143Smoke PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()

# RC1.5.14.5: native Experiment DataTable virtual model + worker contract.
# Exercises 100k retained rows, rolling inserts, edits and no full-reset on overflow.
if(WFSCRIPT_BUILD_TESTS)
    qt_add_executable(WFScriptExperimentTableRC15145Smoke
        tests/experiment_table_rc15145_smoke.cpp
        src/PanelUiBridge.h src/ScriptValueConversion.h
        src/widgets/ExperimentTableData.h
        src/widgets/ExperimentTableModel.h src/widgets/ExperimentTableModel.cpp)
    target_include_directories(WFScriptExperimentTableRC15145Smoke PRIVATE src)
    target_link_libraries(WFScriptExperimentTableRC15145Smoke PRIVATE Qt6::Widgets Qt6::Qml)
    add_test(NAME WFScriptExperimentTableRC15145Smoke COMMAND WFScriptExperimentTableRC15145Smoke)
    set_tests_properties(WFScriptExperimentTableRC15145Smoke PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()

# RC1.5.14.7 — 100k live results + 100k archived results and lazy JSON.
# GUI model/view stress test does not start instruments or the TestRunner.
if(WFSCRIPT_BUILD_TESTS)
    qt_add_executable(WFScriptLargeResultsVirtualRC15147Smoke
        tests/large_results_virtual_rc15147_smoke.cpp
        src/TestSequenceCapacity.h
        src/widgets/MeasurementTableWidget.h src/widgets/MeasurementTableWidget.cpp
        src/TestResultsRepository.h src/TestResultsHistory.cpp
        src/TestResultsTableModels.h src/TestResultsTableModels.cpp
        src/TestResultsBrowserDialog.h src/TestResultsBrowserDialog.cpp)
    target_include_directories(WFScriptLargeResultsVirtualRC15147Smoke PRIVATE src)
    target_link_libraries(WFScriptLargeResultsVirtualRC15147Smoke PRIVATE Qt6::Widgets)
    add_test(NAME WFScriptLargeResultsVirtualRC15147Smoke COMMAND WFScriptLargeResultsVirtualRC15147Smoke)
    set_tests_properties(WFScriptLargeResultsVirtualRC15147Smoke PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()

# RC1.5.14.8 — Measurement Table title/toolbar plus unchanged 100k virtual append.
if(WFSCRIPT_BUILD_TESTS)
    qt_add_executable(WFScriptMeasurementHeaderRC15148Smoke
        tests/measurement_header_rc15148_smoke.cpp
        src/widgets/MeasurementTableWidget.h src/widgets/MeasurementTableWidget.cpp)
    target_include_directories(WFScriptMeasurementHeaderRC15148Smoke PRIVATE src)
    target_link_libraries(WFScriptMeasurementHeaderRC15148Smoke PRIVATE Qt6::Widgets)
    add_test(NAME WFScriptMeasurementHeaderRC15148Smoke COMMAND WFScriptMeasurementHeaderRC15148Smoke)
    set_tests_properties(WFScriptMeasurementHeaderRC15148Smoke PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()

# RC1.5.14.9: appearance propagation on existing and newly created widgets.
# Theme changes must not rebuild high-volume table data or lose checkbox state.
if(WFSCRIPT_BUILD_TESTS)
    qt_add_executable(WFScriptPanelThemeLifecycleRC15149Smoke
        tests/panel_theme_lifecycle_rc15149_smoke.cpp
        src/PanelCanvas.h src/PanelCanvas.cpp
        ${WFSCRIPT_WIDGET_SOURCES}
        src/WaveformPlot.h src/WaveformPlot.cpp
        src/ScopeFrame.h)
    target_include_directories(WFScriptPanelThemeLifecycleRC15149Smoke PRIVATE src)
    target_link_libraries(WFScriptPanelThemeLifecycleRC15149Smoke PRIVATE Qt6::Widgets Qt6::Qml)
    add_test(NAME WFScriptPanelThemeLifecycleRC15149Smoke
             COMMAND WFScriptPanelThemeLifecycleRC15149Smoke)
    set_tests_properties(WFScriptPanelThemeLifecycleRC15149Smoke
                         PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()
