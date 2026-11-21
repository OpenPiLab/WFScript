// One small operator script. All test logic lives in CallbackTestSteps.mjs.
function onCallbackRun(){if(!TestRunner.run())print('TEST RUN: '+TestRunner.lastError);}
function onCallbackStop(){if(!TestRunner.terminate())print('TEST STOP: '+TestRunner.lastError);}
function onCallbackReset(){if(!TestRunner.reset())print('TEST RESET: '+TestRunner.lastError);}
TestRunner.onFinished(function(summary){
    ui.set('stationSummary',String(summary.state).toUpperCase()+' — '+
        summary.passedMeasurements+' passed measurements / '+summary.totalSteps+' steps');
    print('Conditional Flow Lab finished '+JSON.stringify(summary));
});
