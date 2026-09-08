// GPL-3.0. WebGL 2 equivalent of src/renderer.cpp, with vertex lighting.
import {identity,multiply,evaluate,scalar,objectMatrix,cameraMatrix,worldMatrices,worldAt,point,vector,normalized,scale,clamp,f32} from './engine.js';

const vertexSource=`#version 300 es
precision highp float;
layout(location=0) in vec3 position;
layout(location=1) in vec3 normal;
layout(location=2) in vec2 uv;
uniform mat4 world, view, projection;
uniform vec3 ambient, tint;
uniform vec2 velocity;
uniform float alpha, luminosity, diffuse, frame;
uniform bool unlit, reflection;
uniform int lightCount;
uniform vec4 lightPosition[16], lightDirection[16], lightColor[16], lightCone[16];
out vec4 vertexColor;
out vec2 texCoord;
vec3 safeNormalize(vec3 v) {float n=length(v);return n>1e-12?v/n:vec3(0,1,0);}
void main() {
    vec3 p=(world*vec4(position,1)).xyz;
    vec3 n=safeNormalize(mat3(world)*normal);
    vec3 color=vec3(1);
    if(!unlit) {
        color=ambient+vec3(luminosity);
        for(int i=0;i<16;i++) {
            if(i>=lightCount)break;
            vec3 delta=p-lightPosition[i].xyz;
            float distance=length(delta),type=lightPosition[i].w,range=lightColor[i].w;
            vec3 direction=type==0.0?lightDirection[i].xyz:safeNormalize(delta);
            float strength=max(0.0,dot(n,direction))*diffuse;
            if(type!=0.0 && range>0.0)strength*=max(0.0,1.0-distance/range);
            if(type==2.0) {
                float angle=acos(clamp(dot(direction,lightDirection[i].xyz),-1.0,1.0))*57.2957795;
                float cone=lightCone[i].x,edge=lightCone[i].y;
                strength*=edge>0.0?clamp((cone+edge-angle)/edge,0.0,1.0):(angle<cone?1.0:0.0);
            }
            color+=lightColor[i].xyz*strength;
        }
        color=clamp(color,0.0,1.0);
    }
    vertexColor=vec4(color*tint,alpha);
    vec3 vn=safeNormalize(mat3(view)*n);
    texCoord=reflection?vec2(.5+vn.x*.5,.5-vn.y*.5):uv+velocity*frame;
    gl_Position=projection*view*vec4(p,1);
}`;
const fragmentSource=`#version 300 es
precision highp float;
uniform sampler2D image;
uniform bool textured, chroma;
in vec4 vertexColor;
in vec2 texCoord;
out vec4 outputColor;
void main() {
    vec4 c=vertexColor*(textured?texture(image,texCoord):vec4(1));
    if(chroma && c.a<=.1)discard;
    outputColor=c;
}`;

function makeProgram(gl) {
    const compile=(type,source)=>{
        const s=gl.createShader(type);gl.shaderSource(s,source);gl.compileShader(s);
        if(!gl.getShaderParameter(s,gl.COMPILE_STATUS))throw new Error(gl.getShaderInfoLog(s));
        return s;
    };
    const p=gl.createProgram(),vs=compile(gl.VERTEX_SHADER,vertexSource),fs=compile(gl.FRAGMENT_SHADER,fragmentSource);
    gl.attachShader(p,vs);gl.attachShader(p,fs);gl.linkProgram(p);
    if(!gl.getProgramParameter(p,gl.LINK_STATUS))throw new Error(gl.getProgramInfoLog(p));
    gl.deleteShader(vs);gl.deleteShader(fs);return p;
}
const plain={flags:256,textureFlags:0,color:[255,255,255],velocity:[0,0,0],wrapU:1,wrapV:1,
    luminosity:0,diffuse:1,transparency:0,chroma:false,unlit:true,additive:false,alphaOnly:false};
export class Renderer {
    constructor(canvas,data) {
        this.canvas=canvas;this.data=data;this.textures=new Map();this.drawCalls=0;
        const gl=this.gl=canvas.getContext('webgl2',{alpha:false,antialias:false,premultipliedAlpha:false,preserveDrawingBuffer:true});
        if(!gl)throw new Error('WebGL 2 is required. Enable hardware acceleration in your browser.');
        this.program=makeProgram(gl);gl.useProgram(this.program);this.uniforms={};
        for(const n of ['world','view','projection','ambient','tint','velocity','alpha','luminosity','diffuse','frame',
            'unlit','reflection','lightCount','lightPosition','lightDirection','lightColor','lightCone','image','textured','chroma'])
            this.uniforms[n]=gl.getUniformLocation(this.program,n);
        this.meshes=data.meshes.map(m=>{
            const corners=Float32Array.from(m.corners),centers=[];
            for(let t=0;t<m.materials.length;t++) {
                const k=t*24;
                centers.push([0,1,2].map(i=>f32(f32(f32(corners[k+i]+corners[k+8+i])+corners[k+16+i])*f32(1/3))));
            }
            const vao=this.createGeometry(corners);
            return {...m,corners,centers,...vao};
        });
        this.sprite=this.createGeometry(new Float32Array(48),gl.DYNAMIC_DRAW);
        const one=gl.createTexture();gl.bindTexture(gl.TEXTURE_2D,one);
        gl.texImage2D(gl.TEXTURE_2D,0,gl.RGBA,1,1,0,gl.RGBA,gl.UNSIGNED_BYTE,new Uint8Array([255,255,255,255]));
        this.white=one;gl.uniform1i(this.uniforms.image,0);
        this.info={renderer:gl.getParameter(gl.RENDERER),version:gl.getParameter(gl.VERSION)};
    }
    createGeometry(corners,usage=this.gl.STATIC_DRAW) {
        const gl=this.gl,vao=gl.createVertexArray(),buffer=gl.createBuffer();
        gl.bindVertexArray(vao);gl.bindBuffer(gl.ARRAY_BUFFER,buffer);gl.bufferData(gl.ARRAY_BUFFER,corners,usage);
        for(let i=0;i<3;i++){gl.enableVertexAttribArray(i);gl.vertexAttribPointer(i,i===2?2:3,gl.FLOAT,false,32,i===0?0:i===1?12:24);}
        return {vao,buffer};
    }
    async preload(base,progress=()=>{}) {
        const requests=new Map();
        const request=(path,s=plain)=>{
            if(!path)return;
            const key=path+'|'+Number(s.chroma)+'|'+Number(!!(s.textureFlags&16))+'|'+Number(s.alphaOnly);
            if(!requests.has(key))requests.set(key,{path,s});
            return key;
        };
        for(const mesh of this.meshes)for(const s of mesh.surfaces) {
            s.textureKey=request(s.texture,s);s.reflectionKey=request(s.reflection);
        }
        for(const s of this.data.scenes)s.backgroundKey=request(s.background);
        this.sparkKey=request(this.data.spark);this.flareKey=request(this.data.flare);
        let completed=0;
        await Promise.all([...requests].map(async([key,{path,s}])=>{
            const image=new Image();image.src=new URL(path,base).href;await image.decode();
            const cv=document.createElement('canvas');cv.width=image.width;cv.height=image.height;
            const context=cv.getContext('2d',{willReadFrequently:true});context.drawImage(image,0,0);
            const rgba=context.getImageData(0,0,cv.width,cv.height);
            const p=rgba.data;
            for(let i=0;i<p.length;i+=4) {
                if(s.textureFlags&16){p[i]=255-p[i];p[i+1]=255-p[i+1];p[i+2]=255-p[i+2];}
                if(s.chroma)p[i+3]=Math.max(p[i],p[i+1],p[i+2])<16?0:255;
                if(s.alphaOnly){p[i+3]=Math.trunc(255-f32(f32(f32(p[i]*f32(.299))+f32(p[i+1]*f32(.587)))+f32(p[i+2]*f32(.114))));p[i]=p[i+1]=p[i+2]=255;}
            }
            // Typed uploads: explicitly flip rows, preserving alpha-only white RGB.
            const flipped=new Uint8Array(p.length),stride=cv.width*4;
            for(let y=0;y<cv.height;y++)flipped.set(p.subarray(y*stride,(y+1)*stride),(cv.height-1-y)*stride);
            const gl=this.gl,texture=gl.createTexture();gl.bindTexture(gl.TEXTURE_2D,texture);
            gl.pixelStorei(gl.UNPACK_COLORSPACE_CONVERSION_WEBGL,gl.NONE);
            gl.pixelStorei(gl.UNPACK_PREMULTIPLY_ALPHA_WEBGL,false);
            gl.texImage2D(gl.TEXTURE_2D,0,gl.RGBA,cv.width,cv.height,0,gl.RGBA,gl.UNSIGNED_BYTE,flipped);
            gl.texParameteri(gl.TEXTURE_2D,gl.TEXTURE_MIN_FILTER,gl.LINEAR);
            gl.texParameteri(gl.TEXTURE_2D,gl.TEXTURE_MAG_FILTER,gl.LINEAR);
            gl.texParameteri(gl.TEXTURE_2D,gl.TEXTURE_WRAP_S,gl.CLAMP_TO_EDGE);
            gl.texParameteri(gl.TEXTURE_2D,gl.TEXTURE_WRAP_T,gl.CLAMP_TO_EDGE);
            this.textures.set(key,texture);progress(++completed,requests.size);
        }));
    }
    matrix(name,m){this.gl.uniformMatrix4fv(this.uniforms[name],false,m);}
    material(s,alpha,blend=false,override=null,tint=null) {
        const gl=this.gl,u=this.uniforms,reflection=!s.textureKey&&!!s.reflectionKey;
        const texture=override||this.textures.get(reflection?s.reflectionKey:s.textureKey);
        gl.bindTexture(gl.TEXTURE_2D,texture||this.white);
        if(texture) {
            gl.texParameteri(gl.TEXTURE_2D,gl.TEXTURE_WRAP_S,s.wrapU===2&&!reflection?gl.REPEAT:gl.CLAMP_TO_EDGE);
            gl.texParameteri(gl.TEXTURE_2D,gl.TEXTURE_WRAP_T,s.wrapV===2&&!reflection?gl.REPEAT:gl.CLAMP_TO_EDGE);
        }
        if(s.flags&256)gl.disable(gl.CULL_FACE);else gl.enable(gl.CULL_FACE);
        if(blend||s.chroma){gl.enable(gl.BLEND);gl.blendFunc(gl.SRC_ALPHA,s.additive?gl.ONE:gl.ONE_MINUS_SRC_ALPHA);}
        else gl.disable(gl.BLEND);
        gl.uniform1i(u.textured,!!texture);gl.uniform1i(u.reflection,reflection);gl.uniform1i(u.chroma,s.chroma);
        gl.uniform1i(u.unlit,s.unlit||!(s.flags&4));gl.uniform1f(u.alpha,alpha);
        gl.uniform1f(u.luminosity,s.luminosity);gl.uniform1f(u.diffuse,s.diffuse);
        gl.uniform3fv(u.tint,tint||(!texture||s.alphaOnly?s.color.map(v=>v/255):[1,1,1]));
        gl.uniform2fv(u.velocity,s.velocity.slice(0,2));
    }
    quad(x,y,z,rx,ry,flip=false) {
        const vertices=[[x-rx,y-ry,z,0,flip?1:0],[x+rx,y-ry,z,1,flip?1:0],
            [x+rx,y+ry,z,1,flip?0:1],[x-rx,y+ry,z,0,flip?0:1]];
        const data=new Float32Array(48);
        [0,1,2,0,2,3].forEach((j,i)=>{const v=vertices[j];data.set([v[0],v[1],v[2],0,1,0,v[3],v[4]],i*8);});
        const gl=this.gl;gl.bindVertexArray(this.sprite.vao);gl.bindBuffer(gl.ARRAY_BUFFER,this.sprite.buffer);
        gl.bufferSubData(gl.ARRAY_BUFFER,0,data);gl.drawArrays(gl.TRIANGLES,0,6);this.drawCalls++;
    }
    draw(scene,frame) {
        const gl=this.gl,u=this.uniforms,width=this.canvas.width,height=this.canvas.height;
        this.drawCalls=0;gl.useProgram(this.program);gl.disable(gl.SCISSOR_TEST);gl.depthMask(true);
        gl.clearColor(0,0,0,1);gl.clear(gl.COLOR_BUFFER_BIT|gl.DEPTH_BUFFER_BIT);
        const scaleFactor=Math.min(width/640,height/480),cw=Math.trunc(640*scaleFactor),ch=Math.trunc(480*scaleFactor);
        const cx=Math.trunc((width-cw)/2),cy=Math.trunc((height-ch)/2);
        const vw=Math.trunc(Math.min(640,scene.width)*scaleFactor),vh=Math.trunc(Math.min(480,scene.height)*scaleFactor);
        const vx=cx+Math.trunc((cw-vw)/2),vy=cy+Math.trunc((ch-vh)/2);
        gl.enable(gl.SCISSOR_TEST);
        if(scene.background){gl.scissor(cx,cy,cw,ch);gl.clearColor(...scene.backdrop,1);gl.clear(gl.COLOR_BUFFER_BIT);}
        gl.viewport(vx,vy,vw,vh);gl.scissor(vx,vy,vw,vh);gl.clearColor(...scene.backdrop,1);gl.clear(gl.COLOR_BUFFER_BIT);
        gl.disable(gl.DEPTH_TEST);gl.uniform1f(u.frame,frame);this.matrix('world',identity());this.matrix('view',identity());
        if(scene.background) {
            const ortho=new Float32Array([2,0,0,0,0,2,0,0,0,0,-1,0,-1,-1,0,1]);
            this.matrix('projection',ortho);
            this.material(plain,1,false,this.textures.get(scene.backgroundKey));this.quad(.5,.5,0,.5,.5);
        }
        const worlds=worldMatrices(scene,frame),view=cameraMatrix(evaluate(scene.camera,frame));
        const lights=scene.lights.map(l=>{
            let w=objectMatrix(evaluate(l.motion,frame));
            if(l.parent)w=multiply(worlds[l.parent-1],w);
            return {...l,position:point(w),direction:normalized(vector(w,[0,0,1])),rgb:scale(l.color,scalar(l.intensity,frame))};
        });
        if(lights.length>16)throw new Error('Too many lights for the parity shader');
        const lp=new Float32Array(64),ld=new Float32Array(64),lc=new Float32Array(64),ln=new Float32Array(64);
        lights.forEach((l,i)=>{lp.set([...l.position,l.type],i*4);ld.set([...l.direction,0],i*4);lc.set([...l.rgb,l.range],i*4);ln.set([l.cone,l.edge,0,0],i*4);});
        gl.uniform4fv(u.lightPosition,lp);gl.uniform4fv(u.lightDirection,ld);gl.uniform4fv(u.lightColor,lc);gl.uniform4fv(u.lightCone,ln);
        gl.uniform1i(u.lightCount,lights.length);gl.uniform3fv(u.ambient,scale(scene.ambientColor,scalar(scene.ambient,frame)));
        const zoom=Math.max(.001,scalar(scene.zoom,frame)),near=.01,far=200000;
        const projection=new Float32Array([zoom*vh/vw,0,0,0,0,-zoom,0,0,0,0,(far+near)/(far-near),1,0,0,-2*far*near/(far-near),0]);
        this.matrix('projection',projection);this.matrix('view',view);
        gl.enable(gl.DEPTH_TEST);gl.depthFunc(gl.LEQUAL);gl.frontFace(gl.CW);gl.cullFace(gl.BACK);
        const opaque=[],transparent=[];
        scene.objects.forEach((o,i)=>{
            if(o.mesh<0||o.name.includes('[SPARK_'))return;
            const mesh=this.meshes[o.mesh],dissolve=clamp(scalar(o.dissolve,frame)),w=worlds[i];
            mesh.materials.forEach((si,t)=>{
                if(si<0)return;const s=mesh.surfaces[si],alpha=f32(f32(1-dissolve)*f32(1-s.transparency));
                if(alpha<=.001)return;
                const face={mesh,s,alpha,w,t,object:i,depth:point(view,point(w,mesh.centers[t]))[2]};
                (alpha<.999||s.additive||s.alphaOnly?transparent:opaque).push(face);
            });
        });
        transparent.sort((a,b)=>b.depth-a.depth);
        const faces=(list,blend)=>{
            gl.depthMask(!blend);
            for(let i=0;i<list.length;) {
                const f=list[i];let end=i+1;
                while(end<list.length && list[end].object===f.object && list[end].s===f.s && list[end].t===f.t+end-i)end++;
                this.matrix('world',f.w);this.material(f.s,f.alpha,blend);gl.bindVertexArray(f.mesh.vao);
                gl.drawArrays(gl.TRIANGLES,f.t*3,(end-i)*3);this.drawCalls++;i=end;
            }
        };
        faces(opaque,false);faces(transparent,true);
        this.matrix('view',identity());this.matrix('world',identity());gl.depthMask(false);
        const sparkSurface={...plain,additive:true};
        const sprite=(p,radius,color,texture)=>{
            p=point(view,p);if(p[2]<=near)return;
            this.material(sparkSurface,1,true,texture,color);this.quad(p[0],p[1],p[2],radius,radius,true);
        };
        const spark=this.textures.get(this.sparkKey),flare=this.textures.get(this.flareKey);
        scene.objects.forEach((o,i)=>{
            if(!o.name.includes('[SPARK_'))return;
            let seed=1;const random=()=>{seed=(Math.imul(seed,214013)+2531011)>>>0;return(seed>>>16)&32767;};
            const v=evaluate(o.motion,frame);
            for(let j=0;j<150;j++) {
                const delay=random()/80,rx=f32((random()-16383)/655360),rz=f32((random()-16383)/655360);
                if(frame<delay)continue;
                const age=f32((frame-delay)%60),at=point(worldAt(scene,i,frame-age));
                const p=[f32(at[0]+f32(f32(f32(rx*v[6])/3)*age)),
                    f32(at[1]+f32(f32(f32(f32(.02)*v[7])/f32(1.5))*age)),
                    f32(at[2]+f32(f32(f32(rz*v[8])/3)*age))];
                p[1]=f32(p[1]+f32(f32(f32(f32(f32(f32(.0005)*age)*age)*v[7])*scene.gridSize)/30));
                sprite(p,.32,[1,.85,1],spark);
            }
            sprite(point(worlds[i]),.65,[1,.9,1],spark);
        });
        lights.forEach(l=>{if(l.flare>0)sprite(l.position,l.flare*.5,l.rgb,flare);});
        gl.depthMask(true);gl.disable(gl.SCISSOR_TEST);
        const error=gl.getError();if(error!==gl.NO_ERROR)throw new Error('WebGL error '+error);
        return {drawCalls:this.drawCalls,scene:scene.name,frame};
    }
    capture(){return this.canvas.toDataURL('image/png');}
}
