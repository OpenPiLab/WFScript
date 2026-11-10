/**
 * WFScript Example: Digital Timing and Protocol Charts
 * Two Pages of synthetic logic and UART/SPI/I2C timing traces.
 *
 * Device: Simulation by default. Choose Physical Device before Run Panel
 *         for real AD3 operations. No output is enabled merely by opening.
 * Wiring: None. No physical protocol is transmitted or decoded.
 *
 * The script belongs to ONE .wfpanel project. If it contains multiple Pages,
 * the Pages share this JavaScript runtime and globally unique widget IDs.
 * Read the SETTINGS / FUNCTIONS / EVENTS sections from top to bottom.
 * Edit this .js file in the source tree, then run:
 *      python tools/build_examples.py
 * to regenerate the packaged .wfpanel resource and keep JS in sync.
 */

// Shared data-series helpers
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

// Digital timing and protocol demonstrations
function traceDigital(plot,name,times,values,lane,col){
 series(plot,name,times,values.map(v=>v),col,{width:2.5});
}
function logic(){
 const t=range(401,0,.05);
 traceDigital('logicClock','CLK lane 0',t,t.map(v=>Math.floor(v*2)%2),0,PALETTE[0]);
 traceDigital('logicClock','PWM lane 2',t,t.map(v=>(v%2)<.65?1:0),2,PALETTE[1]);
 traceDigital('logicClock','ENABLE lane 4',t,t.map(v=>v>=3&&v<=17?1:0),4,PALETTE[2]);
 traceDigital('logicEdges','Debounce lane 0',t,t.map(v=>v<5?0:(v<5.9?Math.floor((v-5)*15)%2:1)),0,PALETTE[3]);
 traceDigital('logicEdges','Narrow glitch lane 2',t,t.map(v=>v>=8.1&&v<8.4?1:0),2,PALETTE[1]);
 traceDigital('logicEdges','Counter bit lane 4',t,t.map(v=>Math.floor(v/3)%2),4,PALETTE[0]);
 const dt=range(201,0,1);
 series('logicDuty','Sweep',dt,dt.map(v=>10+80*v/200),PALETTE[2]);
 series('logicDuty','Modulated',dt,dt.map(v=>50+35*Math.sin(v/17)),PALETTE[3]);
}
function uartBits(byte){let a=[0];for(let k=0;k<8;k++)a.push((byte>>k)&1);a.push(1);return a;}
function protocol(){
 let ux=range(251,0,.1), seq=[1,1].concat(uartBits(0x55)).concat([1,1]).concat(uartBits(0xA6)).concat([1,1]);
 traceDigital('uartPlot','UART TX',ux,ux.map(v=>seq[Math.min(seq.length-1,Math.floor(v))]),0,PALETTE[0]);
 traceDigital('uartPlot','Frame active',ux,ux.map(v=>((v>=2&&v<12)||(v>=14&&v<24))?1:0),2,PALETTE[1]);
 const sx=range(401,0,.05),out=0xA5,inByte=0x3C;
 const cs=sx.map(v=>v>=2&&v<18?0:1);
 const clk=sx.map(v=>(v>=2&&v<18)?Math.floor((v-2)*2)%2:0);
 const mosi=sx.map(v=>{let i=Math.min(7,Math.floor(Math.max(0,v-2)/2));return (out>>(7-i))&1;});
 const miso=sx.map(v=>{let i=Math.min(7,Math.floor(Math.max(0,v-2)/2));return (inByte>>(7-i))&1;});
 traceDigital('spiPlot','CS active-low',sx,cs,6,PALETTE[1]);
 traceDigital('spiPlot','SCLK',sx,clk,4,PALETTE[0]);
 traceDigital('spiPlot','MOSI A5',sx,mosi,2,PALETTE[2]);
 traceDigital('spiPlot','MISO 3C',sx,miso,0,PALETTE[3]);
 // Synthetic I2C master: START at x=2, eight address bits, ACK=0, STOP at x=22.
 const ix=range(481,0,.05),byte=0xA0;
 const scl=[],sda=[];
 for(const x of ix){
   let clk=1,dat=1;
   if(x>=2&&x<20){
     const slot=Math.min(8,Math.floor((x-2)/2));
     clk=((x-2)%2)<1?0:1;
     dat=slot===8?0:((byte>>(7-slot))&1);
   }else if(x>=20&&x<22){clk=1;dat=0;}
   scl.push(clk);sda.push(dat);
 }
 traceDigital('i2cPlot','SCL',ix,scl,2,PALETTE[0]);
 traceDigital('i2cPlot','SDA',ix,sda,0,PALETTE[1]);
}

function safeAction(name,func){
 ui.on(name,'clicked',function(){
   try{func();print(name+' complete');}
   catch(e){print(name+': '+e);}
 });
}

logic(); protocol();
safeAction('logicRefresh',logic);
safeAction('protoRefresh',protocol);
