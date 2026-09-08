// GPL-3.0. JavaScript port of src/scene.cpp; see ../src/LICENSE.
export const f32 = Math.fround;
export const clamp = (v, lo = 0, hi = 1) => Math.min(hi, Math.max(lo, v));
export const identity = () => new Float32Array([1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1]);
export const add = (a,b) => a.map((v,i) => v+b[i]);
export const scale = (v,s) => v.map(x => x*s);
export const dot = (a,b) => a[0]*b[0]+a[1]*b[1]+a[2]*b[2];
export const normalized = v => {
    const length = Math.sqrt(dot(v,v));
    return length > 1e-12 ? scale(v,1/length) : [0,1,0];
};
export function vector(m,v) {
    return [0,1,2].map(i => f32(f32(f32(m[i]*v[0])+f32(m[4+i]*v[1]))+f32(m[8+i]*v[2])));
}
export function point(m,v = [0,0,0]) {
    return vector(m,v).map((x,i) => f32(x+m[12+i]));
}
export function multiply(a,b) {
    const r = new Float32Array(16);
    for(let j=0;j<4;j++) for(let i=0;i<4;i++) for(let k=0;k<4;k++)
        r[j*4+i] = f32(r[j*4+i]+f32(a[k*4+i]*b[j*4+k]));
    return r;
}
export function evaluate(m, frame) {
    const keys=m.keys;
    if(!keys.length) return m.constant;
    if(keys.length===1) return keys[0].value;
    frame-=m.offset;
    const last=keys[keys.length-1];
    if(m.end===2 && last.frame>0) {frame%=last.frame;if(frame<0)frame+=last.frame;}
    if(frame<=keys[0].frame)return keys[0].value;
    if(frame>=last.frame)return last.value;
    let lo=1,hi=keys.length-1;
    while(lo<hi){const mid=(lo+hi)>>1;if(keys[mid].frame<frame)lo=mid+1;else hi=mid;}
    const index=lo,a=keys[index-1],b=keys[index];
    if(b.frame-a.frame===1)return b.value;
    const length=f32(b.frame-a.frame),t=f32((frame-a.frame)/length);
    const t2=f32(t*t),t3=f32(t2*t);
    const h1=f32(f32(1-f32(3*t2))+f32(2*t3)),h2=f32(f32(3*t2)-f32(2*t3));
    const h3=f32(f32(t3-f32(2*t2))+t),h4=f32(t3-t2);
    const aa=f32(f32(f32(1-a.tension)*f32(1+a.continuity))*f32(1+a.bias));
    const ab=f32(f32(f32(1-a.tension)*f32(1-a.continuity))*f32(1-a.bias));
    const ba=f32(f32(f32(1-b.tension)*f32(1-b.continuity))*f32(1+b.bias));
    const bb=f32(f32(f32(1-b.tension)*f32(1+b.continuity))*f32(1-b.bias));
    const r=new Float32Array(9);
    for(let c=0;c<m.channels;c++) {
        const d=f32(b.value[c]-a.value[c]);
        if(b.linear){r[c]=f32(a.value[c]+f32(t*d));continue;}
        const dd=index===1 ? f32(f32(.5*f32(aa+ab))*d) :
            f32(f32(length/f32(b.frame-keys[index-2].frame))*f32(f32(aa*f32(a.value[c]-keys[index-2].value[c]))+f32(ab*d)));
        const ds=index+1===keys.length ? f32(f32(.5*f32(ba+bb))*d) :
            f32(f32(length/f32(keys[index+1].frame-a.frame))*f32(f32(ba*d)+f32(bb*f32(keys[index+1].value[c]-b.value[c]))));
        r[c]=f32(f32(f32(f32(a.value[c]*h1)+f32(b.value[c]*h2))+f32(dd*h3))+f32(ds*h4));
    }
    return r;
}
export const scalar = (m,frame) => evaluate(m,frame)[0];
export function objectMatrix(v,p=[0,0,0]) {
    const rad=f32(f32(Math.PI)/180);
    const x=f32(-v[4]*rad),y=f32(v[3]*rad),z=f32(-v[5]*rad);
    const sx=f32(Math.sin(x)),cx=f32(Math.cos(x)),sy=f32(Math.sin(y)),cy=f32(Math.cos(y)),sz=f32(Math.sin(z)),cz=f32(Math.cos(z));
    const m=new Float32Array([
        f32(v[6]*f32(f32(cz*cy)+f32(f32(sz*sx)*sy))),f32(v[6]*f32(sz*cx)),f32(v[6]*f32(f32(f32(sz*sx)*cy)-f32(cz*sy))),0,
        f32(v[7]*f32(f32(f32(cz*sx)*sy)-f32(sz*cy))),f32(v[7]*f32(cz*cx)),f32(v[7]*f32(f32(f32(cz*sx)*cy)+f32(sz*sy))),0,
        f32(f32(v[8]*cx)*sy),f32(-f32(v[8]*sx)),f32(f32(v[8]*cx)*cy),0,0,0,0,1]);
    const t=vector(m,[-p[0],p[1],-p[2]]);
    m[12]=f32(t[0]+v[0]);m[13]=f32(t[1]-v[1]);m[14]=f32(t[2]+v[2]);
    return m;
}
export function cameraMatrix(values) {
    const v=Array.from(values);v[6]=v[7]=v[8]=1;
    const w=objectMatrix(v),r=identity();
    for(let j=0;j<3;j++)for(let i=0;i<3;i++)r[j*4+i]=w[i*4+j];
    r.set(vector(r,[-v[0],v[1],-v[2]]),12);return r;
}
export function worldMatrices(scene,frame) {
    const result=new Array(scene.objects.length),state=new Uint8Array(result.length);
    function resolve(i) {
        if(state[i]===2)return;
        if(state[i]===1)throw new Error('Cyclic object parent');
        const o=scene.objects[i];state[i]=1;
        result[i]=objectMatrix(evaluate(o.motion,frame),o.pivot);
        if(o.parent) {
            if(o.parent<1||o.parent>result.length)throw new Error('Invalid object parent');
            resolve(o.parent-1);result[i]=multiply(result[o.parent-1],result[i]);
        }
        state[i]=2;
    }
    scene.objects.forEach((_,i)=>resolve(i));return result;
}
export function worldAt(scene,index,frame) {
    const o=scene.objects[index];let w=objectMatrix(evaluate(o.motion,frame),o.pivot),parent=o.parent;
    while(parent){const p=scene.objects[parent-1];w=multiply(objectMatrix(evaluate(p.motion,frame),p.pivot),w);parent=p.parent;}
    return w;
}
export const cueAt = (scenes,time) => {
    const i=scenes.findIndex(s=>time<s.end);return i<0?scenes.length-1:i;
};
export function sceneFrame(s,time) {
    let local=Math.max(0,time-s.start);
    if(s.hold>=0)local=Math.min(local,s.hold);
    return Math.min(s.last,s.first+local*s.fps);
}
export function prepareData(data) {
    if(data.version!==1||data.duration!==210||data.scenes.length!==11)throw new Error('Unsupported demo data');
    // JSON has decimal representations of native floats; restore their binary32 values.
    function prepare(m) {
        m.constant=Float32Array.from(m.constant);m.offset=f32(m.offset);
        for(const k of m.keys){k.value=Float32Array.from(k.value);for(const p of ['frame','tension','continuity','bias'])k[p]=f32(k[p]);}
    }
    for(const s of data.scenes) {
        [s.camera,s.zoom,s.ambient].forEach(prepare);
        for(const o of s.objects){prepare(o.motion);prepare(o.dissolve);}
        for(const l of s.lights){prepare(l.motion);prepare(l.intensity);}
    }
    return data;
}
