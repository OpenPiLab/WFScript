/**
 * WFScript Example: Analog Charts and Frequency
 * Three Pages: analog time-series, XY and FFT/frequency examples.
 *
 * Device: Simulation by default. Choose Physical Device before Run Panel
 *         for real AD3 operations. No output is enabled merely by opening.
 * Wiring: None. Charts are generated from synthetic values.
 *
 * The script belongs to ONE .wfpanel project. If it contains multiple Pages,
 * the Pages share this JavaScript runtime and globally unique widget IDs.
 * Read the SETTINGS / FUNCTIONS / EVENTS sections from top to bottom.
 * Edit this .js file in the source tree, then run:
 *      python tools/build_examples.py
 * to regenerate the packaged .wfpanel resource and keep JS in sync.
 */

// Shared chart helpers
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

// Analog time plots
function analog(){
 iteration++;
 const t=range(801,0,.01), phase=iteration*.12;
 let ch1=t.map(v=>1.6*Math.sin(2*PI*.75*v+phase)+.25);
 let ch2=t.map(v=>.82*Math.sin(2*PI*.75*v+phase+PI/2)-.18);
 series('analogTime','CH1',t,ch1,PALETTE[0],{width:0});
 series('analogTime','CH2',t,ch2,PALETTE[1],{width:2});
 const s=range(601,0,.02);
 series('analogStep','Step output',s,s.map(v=>v<2?0:1-.65*Math.exp(-(v-2)*.9)*Math.cos((v-2)*8)),PALETTE[2]);
 series('analogEnvelope','Rectified',t,t.map((v,i)=>Math.abs(ch1[i])),PALETTE[3]);
}

// XY data plots
function xy(){
 const n=361;let u=range(n,0,2*PI/(n-1));
 series('xyLissajous','1:2 phase trace',u.map(v=>Math.sin(v)),u.map(v=>Math.sin(2*v+.42)),PALETTE[0],{width:2.3});
 let sweep=[];for(let i=0;i<=240;i++)sweep.push(-2+4*i/240);
 for(let i=240;i>=0;i--)sweep.push(-2+4*i/240);
 series('xyTransfer','Hysteresis loop',sweep,sweep.map((v,i)=>Math.tanh(2.3*v+(i<=240?-.45:.45))),PALETTE[2]);
 let ref=range(81,-4,.1),err=ref.map((x,i)=>.03*x+.05*Math.sin(i*3.17));
 series('xyCloud','Calibration',ref,ref.map((v,i)=>1.018*v+.06+err[i]),PALETTE[1],{markerSize:4.5});
}

// FFT and frequency plots
function fft(){
 // Actual direct DFT of a 512-sample, Hann-windowed synthetic waveform.
 const N=512,fs=100000, samples=[];
 for(let i=0;i<N;i++){
   const t=i/fs;
   samples.push((.9*Math.sin(2*PI*2000*t)+.3*Math.sin(2*PI*8000*t)+.06*Math.sin(2*PI*17100*t)) *
                (.5-.5*Math.cos(2*PI*i/(N-1))));
 }
 const fx=[],db=[];
 for(let k=1;k<N/2;k++){
   let re=0,im=0;
   for(let n=0;n<N;n++){
      const a=2*PI*k*n/N;
      re+=samples[n]*Math.cos(a);im-=samples[n]*Math.sin(a);
   }
   let ampl=4*Math.sqrt(re*re+im*im)/N;
   fx.push(k*fs/N);db.push(20*Math.log10(Math.max(1e-8,ampl)));
 }
 series('fftDb','Hann DFT',fx,db,PALETTE[0],{width:0});
 const freq=[],gain=[],phase=[],mag=[];
 for(let i=0;i<=320;i++){
   const f=Math.pow(10,1+i/80), ratio=f/1500;
   freq.push(f);gain.push(-10*Math.log10(1+ratio*ratio));
   phase.push(-Math.atan(ratio)*180/PI);
   mag.push(1/Math.sqrt(1+ratio*ratio));
 }
 series('bodeGain','Magnitude dB',freq,gain,PALETTE[1]);
 series('bodePhase','Phase',freq,phase,PALETTE[3]);
 series('logPositive','Linear magnitude',freq,mag,PALETTE[2]);
}

function safeAction(name,func){
 ui.on(name,'clicked',function(){
   try{func();print(name+' complete');}
   catch(e){print(name+': '+e);}
 });
}

// Each Page draws into its own chart IDs.
analog(); xy(); fft();
safeAction('analogRefresh',analog);
safeAction('xyRefresh',xy);
safeAction('freqRefresh',fft);
