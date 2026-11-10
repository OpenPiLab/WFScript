/**
 * WFScript Example: Chart Techniques and Analytics
 * Two Pages with analytics, cursor-ready plots, styling and log axes.
 *
 * Device: Simulation by default. Choose Physical Device before Run Panel
 *         for real AD3 operations. No output is enabled merely by opening.
 * Wiring: None. Charts contain generated values.
 *
 * The script belongs to ONE .wfpanel project. If it contains multiple Pages,
 * the Pages share this JavaScript runtime and globally unique widget IDs.
 * Read the SETTINGS / FUNCTIONS / EVENTS sections from top to bottom.
 * Edit this .js file in the source tree, then run:
 *      python tools/build_examples.py
 * to regenerate the packaged .wfpanel resource and keep JS in sync.
 */

// Chart helpers
let iteration=0, trendN=0, captureN=0, styleState=0, visibilityState=true;
const PI=Math.PI;
const PALETTE=['#49c9e4','#f3bd51','#56d6a7','#ef83bd','#ab9cf3','#f17764'];
function series(plot,name,x,y,col,extra){
  const style={color:col||PALETTE[0]};
  if(extra) for(const key in extra) style[key]=extra[key];
  plots.setSeries(plot,name,x,y,style);
}
function range(n,start,step){let a=[];for(let i=0;i<n;i++)a.push(start+i*step);return a;}
function scaledSignal(plot,name,fn,n,step,color){
  let x=range(n,0,step), y=x.map(fn);series(plot,name,x,y,color);return [x,y];
}

// Analytics and trends
function analytics(){
 const bins=range(33,-.8,.05),counts=bins.map(v=>Math.round(70*Math.exp(-.5*Math.pow(v/.22,2))+2));
 series('histogram','Counts',bins,counts,PALETTE[4]);
 const x=range(101,0,.1);
 series('energyArea','Power',x,x.map(v=>90*Math.pow(Math.sin(v*.4),2)*Math.exp(-v*.11)),PALETTE[2]);
 const cx=range(61,-3,.1);
 series('scatterError','Residual',cx,cx.map((v,i)=>1.5*v+1.2*Math.sin(i*1.37)),PALETTE[3],{markerSize:3.5});
 plots.clear('trendPlot');trendN=0;
 for(let i=0;i<45;i++)appendTrend();
}
function appendTrend(){
 const rms=.71+.07*Math.sin(trendN*.19)+.018*Math.cos(trendN*.89);
 const temp=22+1.8*Math.sin(trendN*.12);
 plots.appendPoint('trendPlot','RMS (V)',trendN,rms);
 plots.appendPoint('trendPlot','Temperature (C)',trendN,temp);
 plots.styleSeries('trendPlot','Temperature (C)',{axis:'right'});
 trendN++;
}

// Chart axes and styling
function axes(){
 const x=range(301,-5,1/30);
 series('manualLinear','Sine',x,x.map(v=>1.4*Math.sin(2*v)),PALETTE[0]);
 series('manualLinear','Cosine',x,x.map(v=>1.1*Math.cos(v)),PALETTE[1]);
 const fx=[],pos=[];
 for(let i=0;i<=240;i++){
   let f=Math.pow(10,i/60);
   fx.push(f);pos.push(2/Math.sqrt(1+Math.pow(f/150,2)));
 }
 series('manualLog','Positive signal',fx,pos,PALETTE[2]);
 let tx=range(301,0,.01);
 series('lineWidths','Inherit 5px',tx,tx.map(v=>Math.sin(v*6)),PALETTE[0],{width:0});
 series('lineWidths','Override 1px',tx,tx.map(v=>.55*Math.cos(v*8)),PALETTE[1],{width:1});
 series('lineWidths','Override 3px',tx,tx.map(v=>.25*Math.sin(v*13)),PALETTE[3],{width:3});
 const mx=range(71,0,.1);
 series('visibility','Markers A',mx,mx.map(v=>Math.sin(v*1.8)),PALETTE[4],{markerSize:5});
 series('visibility','Markers B',mx,mx.map(v=>Math.cos(v*1.8)),PALETTE[2],{markerSize:3});
 plots.configure('lineWidths',{lineWidth:5,showLegend:true});
}

function safeAction(name,func){
 ui.on(name,'clicked',function(){
   try{func();print(name+' complete');}
   catch(e){print(name+': '+e);}
 });
}

analytics(); axes();
safeAction('trendAppend',appendTrend);
safeAction('trendReset',analytics);
safeAction('axisToggle',function(){ visibilityState=!visibilityState; plots.styleSeries('visibility','Markers B',{visible:visibilityState}); });
safeAction('axisStyle',function(){ styleState=(styleState+1)%3; plots.configure('lineWidths',{lineWidth:[2,5,8][styleState]}); plots.styleSeries('lineWidths','Inherit 5px',{width:0}); });
