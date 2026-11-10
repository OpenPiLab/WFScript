/**
 * WFScript Example: Editable Calibration Lookup Table
 * Device: Simulation ONLY (no physical instrument API calls).
 * Wiring: None.
 * Simulation: operator-defined sensor volts -> engineering units by piecewise
 *             LINEAR interpolation. Never extrapolate outside entered X range.
 * Table values from ui.get() are NATIVE JavaScript arrays in v5.6.70.2+.
 *
 * Workflow: edit the calibration table, then VALIDATE & PLOT. Move the voltage
 * slider to interpolate; SAVE SAMPLE adds an entry to the results table.
 * Table edits during Run Panel live in runtime state, not the saved .wfpanel.
 * To make permanent defaults, edit Initial Rows in Properties in Design mode.
 */
const FACTORY_POINTS=[
    ['0.50','10.0'],['1.00','25.0'],['2.00','50.0'],
    ['3.00','90.0'],['4.50','140.0']
];
let calibrated=[];
let sampleLog=[];
let dirty=true;
function inform(s){
    ui.set('calStatus',s);
    ui.set('lookupStatus',s);
    print('[Calibration Table] '+s);
}
function parseNumber(cell,row,column,lo,hi){
    const source=String(cell===null||cell===undefined?'':cell).trim();
    const value=Number(source);
    if(!source || !isFinite(value) || value<lo || value>hi)
        throw Error('Row '+row+' '+column+' must be '+lo+' to '+hi);
    return value;
}
function readCalibration(){
    const rows=ui.get('calibrationTable');
    if(!Array.isArray(rows) || rows.length>30)
        throw Error('Expected at most 30 calibration rows from ui.get()');
    const points=[];
    rows.forEach(function(row,index){
        if(!Array.isArray(row) || row.length!==2)
            throw Error('Calibration row '+(index+1)+' must have two cells');
        if(row.every(function(v){return String(v===null||v===undefined?'':v).trim()==='';}))return;
        const x=parseNumber(row[0],index+1,'input (V)',0,5);
        const y=parseNumber(row[1],index+1,'engineering value',-100,250);
        points.push({x:x,y:y});
    });
    if(points.length<2)throw Error('Enter at least two complete calibration points');
    points.sort(function(a,b){return a.x-b.x;});
    for(let i=1;i<points.length;i++)
        if(Math.abs(points[i].x-points[i-1].x)<1e-9)
            throw Error('Repeated input voltage '+points[i].x+' V; each X must be unique');
    return points;
}
function interpolate(points,x){
    if(x<points[0].x || x>points[points.length-1].x) return null;
    for(let i=1;i<points.length;i++){
        const a=points[i-1],b=points[i];
        if(x<=b.x){
            return {value:a.y+(x-a.x)*(b.y-a.y)/(b.x-a.x),segment:i};
        }
    }
    return {value:points[points.length-1].y,segment:points.length-1};
}
function renderCurve(){
    plots.clear('calibrationPlot');
    plots.configure('calibrationPlot',{
        title:'Entered points and piecewise-linear calibration',
        chartType:'XY',xLabel:'Sensor input (V)',yLabel:'Engineering value (°C)',
        showGrid:true,showLegend:true,xAuto:true,yAuto:true
    });
    const xs=calibrated.map(function(p){return p.x;});
    const ys=calibrated.map(function(p){return p.y;});
    plots.setSeries('calibrationPlot','Interpolated calibration',xs,ys,
                    {color:'#44c8e4',width:2.3});
    plots.setSeries('calibrationPlot','Entered calibration points',xs,ys,
                    {color:'#f3bf52',width:0.5,markerSize:7.0});
}
function sampleAtInput(){
    if(dirty || calibrated.length<2){
        inform('Calibration edited. Press VALIDATE & PLOT before sampling.');
        return null;
    }
    const x=Number(ui.get('probeInput'));
    if(!isFinite(x))throw Error('Invalid sensor input');
    ui.set('inputReadout',x);
    const result=interpolate(calibrated,x);
    if(result===null){
        inform('OUT OF RANGE: '+x.toFixed(2)+' V outside '+
               calibrated[0].x.toFixed(2)+'–'+calibrated[calibrated.length-1].x.toFixed(2)+' V.');
        return null;
    }
    ui.set('estimatedValue',result.value);
    plots.setSeries('calibrationPlot','Selected input',[x],[result.value],
                    {color:'#ef89ba',width:1.0,markerSize:9});
    inform('INTERPOLATED '+x.toFixed(3)+' V -> '+result.value.toFixed(2)+
           ' °C using segment '+result.segment+'/'+(calibrated.length-1)+'.');
    return {x:x,temperature:result.value,segment:result.segment};
}
function validateAndPlot(){
    calibrated=readCalibration();
    dirty=false;
    renderCurve();
    const result=sampleAtInput();
    if(result===null)inform('VALIDATED '+calibrated.length+' calibration points; select an in-range voltage.');
}
function saveSample(){
    const sample=sampleAtInput();
    if(!sample)return;
    if(sampleLog.length>=100)throw Error('Maximum 100 sample-log rows');
    sampleLog.push([String(sampleLog.length+1),sample.x.toFixed(3),
                    sample.temperature.toFixed(2),String(sample.segment)]);
    ui.set('sampleLog',sampleLog);
    inform('SAVED '+sampleLog.length+' calibrated sample(s); right-click the result table for CSV.');
}
function restore(){
    ui.set('calibrationTable',FACTORY_POINTS.map(function(r){return r.slice();}));
    sampleLog=[];ui.set('sampleLog',[]);
    validateAndPlot();
    inform('Factory calibration points restored; no physical input or output.');
}
function safeClick(id,fn){
    ui.on(id,'clicked',function(){try{fn();}catch(e){inform('ERROR: '+String(e));}});
}
safeClick('validateCalibration',validateAndPlot);
safeClick('restoreCalibration',restore);
safeClick('saveSample',saveSample);
safeClick('clearSamples',function(){sampleLog=[];ui.set('sampleLog',[]);inform('Sample log cleared.');});
ui.on('calibrationTable','valueChanged',function(rows){
    dirty=true;
    calibrated=[];
    plots.clear('calibrationPlot');
    inform('EDITED '+rows.length+' rows. Press VALIDATE & PLOT to apply the new calibration.');
});
ui.on('probeInput','valueChanged',function(){try{sampleAtInput();}catch(e){inform('ERROR: '+String(e));}});
ui.set('sampleLog',[]);
try{validateAndPlot();}catch(e){inform('ERROR: '+String(e));}
