// GPL-3.0. Seekable Web Audio clock using the native libxm PCM export.
export class AudioClock {
    constructor() {
        this.context=new AudioContext({sampleRate:48000,latencyHint:'interactive'});
        this.source=null;this.buffer=null;this.offset=0;this.started=0;this.playing=false;
        this.desiredPlayback=false;this.generation=0;
        this.muted=false;this.gain=this.context.createGain();this.gain.connect(this.context.destination);
    }
    async load(url) {
        const response=await fetch(url);
        if(!response.ok)throw new Error('Cannot load soundtrack: HTTP '+response.status);
        this.buffer=await this.context.decodeAudioData(await response.arrayBuffer());
        if(Math.abs(this.buffer.duration-210)>.01)throw new Error('Soundtrack duration mismatch');
    }
    get duration(){return this.buffer?.duration??210;}
    time(){return Math.min(this.duration,this.offset+(this.playing?this.context.currentTime-this.started:0));}
    async play() {
        if(!this.buffer||this.playing)return;
        this.desiredPlayback=true;const generation=++this.generation;
        await this.context.resume();
        // A pause/capture/another seek can cancel a pending browser audio unlock.
        if(!this.desiredPlayback||generation!==this.generation)return;
        if(this.context.state!=='running')throw new Error('Audio is suspended. Press Play to resume.');
        if(this.offset>=this.duration)this.offset=0;
        this.source=this.context.createBufferSource();this.source.buffer=this.buffer;
        this.source.connect(this.gain);this.started=this.context.currentTime;this.playing=true;
        this.source.start(this.started,this.offset);
    }
    pause() {
        this.offset=this.time();this.playing=false;
        this.desiredPlayback=false;this.generation++;
        if(this.source){this.source.stop();this.source.disconnect();this.source=null;}
    }
    async seek(seconds) {
        const wasPlaying=this.playing||this.desiredPlayback;this.pause();this.offset=Math.max(0,Math.min(this.duration,seconds));
        if(wasPlaying&&this.offset<this.duration)await this.play();
    }
    mute(value){this.muted=value;this.gain.gain.setValueAtTime(value?0:1,this.context.currentTime);}
    status() {return {time:this.time(),playing:this.playing,muted:this.muted,state:this.context.state,
        sampleRate:this.context.sampleRate,audioDuration:this.duration};}
}
