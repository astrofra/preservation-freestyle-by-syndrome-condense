// GPL-3.0. Browser player and deterministic capture API.
import {prepareData,cueAt,sceneFrame,worldMatrices,cameraMatrix,evaluate} from './engine.js';
import {Renderer} from './renderer.js';
import {AudioClock} from './audio.js';
const $=id=>document.getElementById(id),params=new URLSearchParams(location.search);
const canvas=$('demo');let renderer,data,audio,ready=false,raf=0,previousScene=-1,lastFrame=null;
const transitions=[];
let renderTimes=[],frameIntervals=[],lastTick=null;
const format=t=>Math.floor(t/60)+':'+String(Math.floor(t%60)).padStart(2,'0');
function error(e) {
    console.error(e);$('error').hidden=false;$('error').textContent=e.message||String(e);
    $('message').textContent='Freestyle could not start. '+(e.message||e);
    window.freestyleError=String(e);
}
function render(time) {
    const index=cueAt(data.scenes,time),scene=data.scenes[index],frame=sceneFrame(scene,time);
    lastFrame={...renderer.draw(scene,frame),time,index};
    if(index!==previousScene){transitions.push({time,scene:scene.name,index});previousScene=index;}
    $('time').value=format(time)+' / 3:30';$('seek').value=time;
    $('play').textContent=audio.playing?'Pause':'Play';
    return lastFrame;
}
function loop() {
    try {
        if(audio.playing){
            const before=performance.now(),t=audio.time();render(t);renderTimes.push(performance.now()-before);
            if(lastTick!==null)frameIntervals.push(before-lastTick);lastTick=before;
            if(t>=210){audio.pause();$('play').textContent='Replay';}
        }else lastTick=null;
        raf=requestAnimationFrame(loop);
    }catch(e){audio.pause();error(e);}
}
async function play() {
    if(!ready)return;
    await audio.play();$('intro').hidden=true;render(audio.time());
}
async function toggle(){if(audio.playing||audio.desiredPlayback){audio.pause();render(audio.time());}else await play();}
async function seek(time){await audio.seek(time);render(audio.time());}
function captureDownload() {
    render(audio.time());
    const a=document.createElement('a');a.download='freestyle_'+String(Math.round(audio.time()*1000)).padStart(9,'0')+'ms.png';
    a.href=renderer.capture();a.click();
}
async function fullscreen() {
    if(document.fullscreenElement)await document.exitFullscreen();
    else await $('stage').requestFullscreen();
}
function resize() {
    if(params.has('capture'))return;
    const box=canvas.getBoundingClientRect(),ratio=Math.min(devicePixelRatio||1,2);
    const width=Math.max(64,Math.round(box.width*ratio)),height=Math.max(64,Math.round(box.height*ratio));
    if(canvas.width!==width||canvas.height!==height){canvas.width=width;canvas.height=height;if(ready)render(audio.time());}
}
async function boot() {
    const width=Number(params.get('width')||640),height=Number(params.get('height')||480);
    if(!Number.isInteger(width)||!Number.isInteger(height)||width<64||height<64||width>4096||height>4096)throw new Error('Invalid capture dimensions');
    canvas.width=width;canvas.height=height;
    const response=await fetch('./assets/demo.json');if(!response.ok)throw new Error('Assets missing. Run tools/build_web.py first.');
    data=prepareData(await response.json());renderer=new Renderer(canvas,data);audio=new AudioClock();audio.onError=error;
    const base=new URL('./assets/',location.href);
    await Promise.all([
        renderer.preload(base,(n,total)=>{$('loading').value=n/total*.5;$('message').textContent='Loading artwork… '+n+' / '+total;}),
        audio.load(new URL('Mush.xm',base))
    ]);
    ready=true;$('loading').value=1;$('loading').hidden=true;$('message').textContent='Freestyle · 3 min 30';
    for(const id of ['start','play','restart','seek','mute','capture','fullscreen'])$(id).disabled=false;
    $('start').onclick=()=>play().catch(error);$('play').onclick=()=>toggle().catch(error);
    $('restart').onclick=()=>seek(0).catch(error);$('seek').oninput=e=>seek(Number(e.target.value)).catch(error);
    $('mute').onclick=()=>{audio.mute(!audio.muted);$('mute').textContent=audio.muted?'Unmute':'Mute';$('mute').setAttribute('aria-pressed',String(audio.muted));};
    $('capture').onclick=captureDownload;$('fullscreen').onclick=()=>fullscreen().catch(error);
    document.addEventListener('keydown',e=>{
        if(e.target instanceof HTMLInputElement)return;
        if(['Space','ArrowLeft','ArrowRight','Home','KeyF','KeyC'].includes(e.code))e.preventDefault();
        if(e.code==='Space')toggle().catch(error);
        if(e.code==='ArrowLeft')seek(audio.time()-5).catch(error);
        if(e.code==='ArrowRight')seek(audio.time()+5).catch(error);
        if(e.code==='Home')seek(0).catch(error);
        if(e.code==='KeyF')fullscreen().catch(error);
        if(e.code==='KeyC')captureDownload();
    });
    canvas.addEventListener('webglcontextlost',e=>{e.preventDefault();audio.pause();cancelAnimationFrame(raf);error(new Error('Graphics context lost. Reload this page to restore the demo.'));});
    if(params.has('mute')){
        audio.mute(true);$('mute').textContent='Unmute';$('mute').setAttribute('aria-pressed','true');
    }
    const time=Number(params.get('time')||0);
    if(!Number.isFinite(time)||time<0||time>210)throw new Error('Invalid demo time');
    await audio.seek(time);render(time);resize();new ResizeObserver(resize).observe(canvas);
    window.freestyle={
        ready:true,version:1,duration:data.duration,play,pause:()=>{audio.pause();render(audio.time());},seek,
        status:()=>({...audio.status(),...lastFrame,transitions:[...transitions],graphics:renderer.info}),
        resetMetrics:()=>{renderTimes=[];frameIntervals=[];lastTick=null;transitions.length=0;previousScene=-1;audio.resetMetrics();},
        metrics:()=>{
            const summary=a=>{const sorted=[...a].sort((x,y)=>x-y);return {count:a.length,
                mean:a.length?a.reduce((x,y)=>x+y,0)/a.length:0,p95:sorted[Math.floor(sorted.length*.95)]||0,max:sorted.at(-1)||0};};
            return {renderMs:summary(renderTimes),frameIntervalMs:summary(frameIntervals),transitions:[...transitions]};
        },
        captureAt:async time=>{
            if(!Number.isFinite(time)||time<0||time>=210)throw new Error('Capture time must be in [0,210)');
            audio.pause();await audio.seek(time);const metadata=render(time);
            return {...metadata,png:renderer.capture()};
        },
        validateMotion:async()=>{
            const r=await fetch('./assets/motion-reference.json');
            if(!r.ok)throw new Error('Native motion oracle is absent');
            const references=await r.json();let maxError=0,values=0,frameError=0,worst=null;
            const compare=(a,b,at)=>{a.forEach((v,i)=>{const d=Math.abs(v-b[i]);if(d>maxError){maxError=d;worst={...at,element:i,actual:v,expected:b[i]};}values++;});};
            for(const ref of references) {
                const s=data.scenes[ref.scene],frame=sceneFrame(s,ref.time);frameError=Math.max(frameError,Math.abs(frame-ref.frame));
                compare(cameraMatrix(evaluate(s.camera,frame)),ref.camera,{time:ref.time,object:'camera'});
                worldMatrices(s,frame).forEach((w,i)=>compare(w,ref.worlds[i],{time:ref.time,object:i}));
            }
            return {samples:references.length,values,maxError,frameError,worst};
        },
        validateAudio:()=>audio.validateAudio()
    };
    requestAnimationFrame(loop);
}
boot().catch(error);
