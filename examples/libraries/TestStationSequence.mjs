/**
 * WFScript v5.6.61: reusable versioned sequence definition.
 * Data only: step order, groups, timing, named exported functions.
 * TestStationSteps.mjs contains the executable implementations.
 * The same definition is also provided as ControllerFunctionalTest.wftest.json.
 * Qt/C++ validates and binds every export atomically before replacing a run.
 */
export const definition = {
  "schemaVersion": 1,
  "id": "controller-functional-demo",
  "title": "Controller Functional Test",
  "config": {
    "sequenceId": "stationSequence",
    "tableId": "stationTable",
    "logId": "stationLog",
    "summaryId": "stationSummary",
    "stopOnFailure": true,
    "totalTimeoutMs": 5000
  },
  "steps": [
    {
      "id": "initialize",
      "name": "SETUP — Initialize test station",
      "group": "setup",
      "type": "action",
      "description": "Qt nonblocking 150 ms settling delay",
      "mode": "delay",
      "durationMs": 150,
      "timeoutMs": 600
    },
    {
      "id": "positive",
      "name": "Verify V+ supply",
      "group": "main",
      "type": "test",
      "description": "One-shot: 3.301 V; 3.25 ... 3.35 V",
      "mode": "once",
      "handler": "verifyPositive"
    },
    {
      "id": "negative",
      "name": "Verify V− supply",
      "group": "main",
      "type": "test",
      "description": "One-shot: -3.298 V; -3.35 ... -3.25 V",
      "mode": "once",
      "handler": "verifyNegative"
    },
    {
      "id": "clock",
      "name": "Verify clock frequency",
      "group": "main",
      "type": "test",
      "description": "Poll every ~10 ms for 180 ms; 800 ms timeout",
      "mode": "poll",
      "intervalMs": 10,
      "timeoutMs": 800,
      "handlers": {
        "start": "beginClock",
        "poll": "pollClock",
        "finish": "endClock"
      }
    },
    {
      "id": "pwm",
      "name": "Verify PWM duty cycle",
      "group": "main",
      "type": "test",
      "description": "Nominal 50.1% PASS; injected 47.5% FAIL",
      "mode": "once",
      "handler": "verifyPwm"
    },
    {
      "id": "report",
      "name": "Finalize test summary",
      "group": "main",
      "type": "test",
      "description": "Read four results and two shared C++ variables",
      "mode": "once",
      "handler": "finalize"
    },
    {
      "id": "cleanup",
      "name": "CLEANUP — Restore safe station state",
      "group": "cleanup",
      "type": "action",
      "description": "Runs on failure/timeout/terminate; abort skips it",
      "mode": "once",
      "timeoutMs": 500,
      "handler": "restoreStation"
    }
  ]
};

// Scenario-specific settling time without modifying the baseline definition.
export function withSettle(durationMs) {
    const copy=JSON.parse(JSON.stringify(definition));
    const step=copy.steps.find(s=>s.id==='initialize');
    step.durationMs=durationMs;
    step.timeoutMs=durationMs+450;
    step.description='Qt nonblocking '+durationMs+' ms settling delay';
    return copy;
}
