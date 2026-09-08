// GPL-3.0. Bounded-state libxm decoder; all tracker state stays inside WASM.
export const RATE=48000, END=210*RATE, BLOCK=4096, CHECKPOINT=30*RATE;
export class XmDecoder {
    static async create(wasm,xm) {
        const module=wasm instanceof WebAssembly.Module?wasm:await WebAssembly.compile(wasm);
        const instance=await WebAssembly.instantiate(module,{});
        const api=instance.exports;api._initialize();
        new Uint8Array(api.memory.buffer).set(xm,api.fs_input());
        if(api.fs_load(xm.byteLength,RATE))throw new Error('libxm rejected Mush.xm or ran out of memory');
        return new XmDecoder(api);
    }
    constructor(api) {
        this.api=api;this.frame=0;this.checkpoints=new Map();
        this.save();
    }
    save() {
        // Full linear-memory copies also preserve libxm's file-static PRNG.
        this.checkpoints.set(this.frame,new Uint8Array(this.api.memory.buffer).slice());
        while(this.checkpoints.size>3) {
            const oldest=[...this.checkpoints.keys()].find(k=>k!==0);
            this.checkpoints.delete(oldest);
        }
    }
    render(request=BLOCK) {
        const count=Math.min(request,BLOCK,END-this.frame,CHECKPOINT-this.frame%CHECKPOINT);
        if(count<=0)return new Float32Array();
        if(this.api.fs_render(count))throw new Error('libxm rendering failed');
        const pcm=new Float32Array(this.api.memory.buffer,this.api.fs_buffer(),count*2).slice();
        this.frame+=count;
        if(this.frame%CHECKPOINT===0)this.save();
        return pcm;
    }
    async seek(target,valid=()=>true) {
        target=Math.max(0,Math.min(END,Math.round(target)));
        if(this.frame>target) {
            const best=Math.max(...[...this.checkpoints.keys()].filter(k=>k<=target));
            new Uint8Array(this.api.memory.buffer).set(this.checkpoints.get(best));this.frame=best;
        }else {
            const best=Math.max(...[...this.checkpoints.keys()].filter(k=>k<=target));
            if(best>this.frame){new Uint8Array(this.api.memory.buffer).set(this.checkpoints.get(best));this.frame=best;}
        }
        let batch=0;
        while(this.frame<target) {
            if(!valid())return false;
            this.render(Math.min(BLOCK,target-this.frame));
            if(++batch%32===0)await new Promise(resolve=>setTimeout(resolve,0));
        }
        return valid();
    }
    info(){return {memoryBytes:this.api.memory.buffer.byteLength,contextBytes:this.api.fs_context_size(),
        checkpointBytes:[...this.checkpoints.values()].reduce((n,c)=>n+c.byteLength,0),checkpointCount:this.checkpoints.size};}
}
