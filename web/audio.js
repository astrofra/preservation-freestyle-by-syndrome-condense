// GPL-3.0. Direct XM playback: libxm WASM worker -> bounded AudioWorklet queue.
const RATE=48000,DURATION=210;
export class AudioClock {
    constructor() {
        this.context=new AudioContext({sampleRate:RATE,latencyHint:'interactive'});
        this.offset=0;this.started=0;this.playing=false;this.advancing=false;
        this.desiredPlayback=false;this.generation=0;this.waiting=new Map();this.requests=new Map();this.requestId=0;
        this.muted=false;this.loaded=false;this.stats={};this.decoderInfo={};this.onError=()=>{};
        this.gain=this.context.createGain();this.gain.connect(this.context.destination);
    }
    async load(url) {
        if(!this.context.audioWorklet)throw new Error('AudioWorklet requires HTTPS or localhost.');
        const get=async u=>{const r=await fetch(u);if(!r.ok)throw new Error('Cannot load '+u+': HTTP '+r.status);return r.arrayBuffer();};
        const [xm,wasm]=await Promise.all([get(url),get(new URL('./libxm.wasm',import.meta.url)),
            this.context.audioWorklet.addModule(new URL('./xm-worklet.js',import.meta.url))]);
        this.node=new AudioWorkletNode(this.context,'freestyle-xm-stream',
            {numberOfInputs:0,numberOfOutputs:1,outputChannelCount:[2]});
        this.node.connect(this.gain);this.node.onprocessorerror=()=>this.fail(new Error('Audio processor failed'));
        this.worker=new Worker(new URL('./xm-worker.js',import.meta.url),{type:'module'});
        this.worker.onerror=e=>this.fail(new Error(e.message||'XM decoder worker failed'));
        const ready=new Promise((resolve,reject)=>{this.readyResolve=resolve;this.readyReject=reject;});
        this.worker.onmessage=({data:m})=>{
            if(m.type==='ready'){this.decoderInfo=m.info;this.readyResolve();}
            if(m.type==='prepared'&&m.generation===this.generation)this.decoderInfo=m.info;
            if(m.type==='error')this.fail(new Error(m.message));
            if(m.type==='validation'){this.requests.get(m.id)?.resolve(m);this.requests.delete(m.id);}
        };
        this.node.port.onmessage=({data:m})=>{
            if(m.generation!==this.generation)return;
            this.stats=m.stats;
            if(!this.desiredPlayback)return;
            this.offset=m.frame/RATE;this.started=m.contextTime;this.advancing=m.running;
            if(m.type==='started'){
                this.playing=true;this.waiting.get(m.generation)?.resolve();this.waiting.delete(m.generation);
            }
            if(m.type==='ended'){this.offset=DURATION;this.advancing=false;}
        };
        const channel=new MessageChannel();
        this.node.port.postMessage({type:'connect',port:channel.port1},[channel.port1]);
        this.worker.postMessage({type:'init',xm,wasm,port:channel.port2},[xm,wasm,channel.port2]);
        await ready;this.loaded=true;
    }
    get duration(){return DURATION;}
    time(){
        const delta=this.playing&&this.advancing?this.context.currentTime-this.started:0;
        return Math.max(0,Math.min(DURATION,this.offset+delta));
    }
    async play() {
        if(!this.loaded||this.playing)return;
        this.desiredPlayback=true;const generation=++this.generation;
        await this.context.resume();
        if(!this.desiredPlayback||generation!==this.generation)return;
        if(this.context.state!=='running')throw new Error('Audio is suspended. Press Play to resume.');
        if(this.offset>=DURATION)this.offset=0;
        const frame=Math.round(this.offset*RATE);
        const started=new Promise((resolve,reject)=>this.waiting.set(generation,{resolve,reject}));
        this.node.port.postMessage({type:'reset',generation,frame});
        this.worker.postMessage({type:'seek',generation,frame});
        await started;
    }
    pause() {
        this.offset=this.time();this.playing=false;this.advancing=false;this.desiredPlayback=false;
        const generation=++this.generation;
        for(const p of this.waiting.values())p.resolve();this.waiting.clear();
        this.node?.port.postMessage({type:'pause',generation,frame:Math.round(this.offset*RATE)});
        this.worker?.postMessage({type:'cancel',generation});
    }
    async seek(seconds) {
        if(!Number.isFinite(seconds))throw new Error('Invalid seek time');
        const wasPlaying=this.playing||this.desiredPlayback;this.pause();
        this.offset=Math.round(Math.max(0,Math.min(DURATION,seconds))*RATE)/RATE;
        if(wasPlaying&&this.offset<DURATION)await this.play();
    }
    mute(value){this.muted=value;this.gain.gain.setValueAtTime(value?0:1,this.context.currentTime);}
    resetMetrics(){this.stats={};this.node.port.postMessage({type:'reset-metrics'});}
    status(){return {time:this.time(),playing:this.playing,muted:this.muted,state:this.context.state,
        sampleRate:RATE,audioDuration:DURATION,backend:'libxm v0.2 / WebAssembly / AudioWorklet',
        streaming:{...this.stats,...this.decoderInfo}};}
    validateAudio() {
        if(this.playing||this.desiredPlayback)throw new Error('Pause before running offline audio validation');
        const id=++this.requestId;
        const result=new Promise((resolve,reject)=>this.requests.set(id,{resolve,reject}));
        this.worker.postMessage({type:'validate',id});return result;
    }
    fail(error) {
        for(const p of this.waiting.values())p.reject(error);this.waiting.clear();
        this.pause();this.readyReject?.(error);
        for(const p of this.requests.values())p.reject(error);this.requests.clear();
        this.onError(error);
    }
}
