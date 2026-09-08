// GPL-3.0. Bounded audio-thread PCM queue; decoding stays in a dedicated worker.
class XmStream extends AudioWorkletProcessor {
    constructor() {
        super();this.queue=[];this.queued=0;this.offset=0;this.frame=0;this.generation=0;
        this.wanted=false;this.running=false;this.pending=false;this.decodedEnd=false;
        this.played=0;this.underruns=0;this.discontinuities=0;this.peak=0;this.notify=0;
        this.port.onmessage=({data:m})=>{
            if(m.type==='connect'){
                this.decoder=m.port;this.decoder.onmessage=({data:n})=>this.receive(n);this.decoder.start();
            }else if(m.type==='reset'||m.type==='pause') {
                this.generation=m.generation;this.frame=m.frame;this.queue=[];this.queued=0;this.offset=0;
                this.wanted=m.type==='reset';this.running=false;this.pending=true;this.decodedEnd=false;
                this.state('state');
            }else if(m.type==='reset-metrics'){this.played=0;this.underruns=0;this.discontinuities=0;this.peak=this.queued;}
        };
    }
    receive(m) {
        if(m.generation!==this.generation)return;
        if(m.type==='pcm') {
            if(m.start!==this.frame+this.queued){this.discontinuities++;return;}
            this.queue.push(m.pcm);this.queued+=m.pcm.length/2;this.peak=Math.max(this.peak,this.queued);
        }else if(m.type==='filled'){this.pending=false;this.decodedEnd=m.ended;}
    }
    state(type,outputFrames=0) {
        this.port.postMessage({type,generation:this.generation,frame:this.frame,contextTime:currentTime+outputFrames/sampleRate,
            running:this.running,stats:{playedFrames:this.played,underruns:this.underruns,
                discontinuities:this.discontinuities,peakQueuedFrames:this.peak,queuedFrames:this.queued}});
    }
    process(_inputs,outputs) {
        const [left,right]=outputs[0];if(!left)return true;
        if(this.wanted&&!this.running&&this.frame<10080000&&this.queued>=Math.min(8192,10080000-this.frame)) {
            this.running=true;this.state('started');
        }
        if(!this.running)return true;
        let out=0;
        while(out<left.length&&this.queued>0) {
            const chunk=this.queue[0],available=chunk.length/2-this.offset;
            const n=Math.min(left.length-out,available);
            for(let i=0;i<n;i++){left[out+i]=chunk[(this.offset+i)*2];right[out+i]=chunk[(this.offset+i)*2+1];}
            out+=n;this.offset+=n;this.queued-=n;this.frame+=n;this.played+=n;
            if(this.offset===chunk.length/2){this.queue.shift();this.offset=0;}
        }
        if(this.frame>=10080000){this.wanted=false;this.running=false;this.state('ended',out);}
        else if(out<left.length){this.underruns++;this.running=false;this.state('state',out);}
        if(!this.pending&&!this.decodedEnd&&this.queued<8192) {
            this.pending=true;this.decoder.postMessage({type:'need',generation:this.generation,count:24576-this.queued});
        }
        if(++this.notify%16===0)this.state('state',out);
        return true;
    }
}
registerProcessor('freestyle-xm-stream',XmStream);
