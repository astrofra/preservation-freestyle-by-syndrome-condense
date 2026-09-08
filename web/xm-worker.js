// GPL-3.0. XM decoding off the graphics/audio threads; transferable PCM blocks.
import {XmDecoder,RATE,END,BLOCK} from './xm-decoder.js';
let decoder,channel,generation=0,seeking=false,module,xm;
function fail(error,id=null){postMessage({type:'error',id,message:error.message||String(error)});}
function fill(count,g) {
    if(g!==generation||seeking)return;
    let left=Math.min(count,24576);
    while(left>0&&decoder.frame<END) {
        const start=decoder.frame,pcm=decoder.render(Math.min(left,BLOCK));left-=pcm.length/2;
        channel.postMessage({type:'pcm',generation:g,start,pcm},[pcm.buffer]);
    }
    channel.postMessage({type:'filled',generation:g,ended:decoder.frame===END});
}
onmessage=async({data:m})=>{
    try {
        if(m.type==='init') {
            module=await WebAssembly.compile(m.wasm);xm=new Uint8Array(m.xm);
            decoder=await XmDecoder.create(module,xm);channel=m.port;
            channel.onmessage=({data:n})=>{try{if(n.type==='need')fill(n.count,n.generation);}catch(e){fail(e);}};
            channel.start();postMessage({type:'ready',info:decoder.info()});
        }else if(m.type==='cancel') {generation=m.generation;seeking=false;}
        else if(m.type==='seek') {
            generation=m.generation;seeking=true;const g=generation;
            const ok=await decoder.seek(m.frame,()=>generation===g);
            if(!ok)return;
            seeking=false;fill(16384,g);postMessage({type:'prepared',generation:g,info:decoder.info()});
        }else if(m.type==='validate') {
            // QA only, never part of playback: reproduce the native PCM16 export.
            const test=await XmDecoder.create(module,xm),pcm16=new Int16Array(END*2);
            let offset=0;
            while(test.frame<END) {
                const chunk=test.render();
                for(let i=0;i<chunk.length;i++)pcm16[offset+i]=Math.fround(Math.max(-1,Math.min(1,chunk[i]))*32767);
                offset+=chunk.length;
            }
            const hash=await crypto.subtle.digest('SHA-256',pcm16);
            postMessage({type:'validation',id:m.id,frames:END,sampleRate:RATE,
                pcmSha256:Array.from(new Uint8Array(hash),v=>v.toString(16).padStart(2,'0')).join('')});
        }
    }catch(error){fail(error,m.id);}
};
