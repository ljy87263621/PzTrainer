#pragma once

namespace pztrainer::features::visual {
inline constexpr const char* kModelCompositeVertex = R"PZCOMP_VERT(#version 330
out vec2 uv;
void main() {
    vec2 corner=vec2((gl_VertexID<<1)&2,gl_VertexID&2);
    uv=corner;
    gl_Position=vec4(corner*2.0-1.0,0.0,1.0);
}
)PZCOMP_VERT";
inline constexpr const char* kModelCompositeFragment = R"PZCOMP_FRAG(#version 330
in vec2 uv;
out vec4 colour;
uniform sampler2D maskTexture;
uniform sampler2D fillTexture;
uniform bool fillEnabled;
uniform bool fillStraight;
uniform bool excludeEnabled;
uniform sampler2D excludeTexture;
uniform vec4 edgeColour;
uniform vec2 pixelSize;
uniform vec3 panini;
uniform vec2 paniniTan;
vec2 frameUv(vec2 screenUv) {
    if(panini.x<=0.0) return screenUv;
    vec2 p=(screenUv*2.0-1.0)*panini.yz;
    float d=panini.x;
    float q=p.x/(1.0+d);
    float q2=q*q;
    float c=(sqrt(max(0.0,1.0+q2*(1.0-d*d)))-d*q2)/(1.0+q2);
    return p*((d+c)/((1.0+d)*max(c,0.0001)))/paniniTan*0.5+0.5;
}
bool inside(vec2 p) { return all(greaterThanEqual(p,vec2(0))) && all(lessThanEqual(p,vec2(1))); }
float silhouette(vec2 screenUv) {
    vec2 p=frameUv(screenUv);
    return inside(p)?texture(maskTexture,p).a:0.0;
}
void main() {
    if(excludeEnabled && texture(excludeTexture,uv).a>0.01)discard;
    vec2 p=frameUv(uv);
    vec4 fill=fillEnabled && inside(p)?texture(fillTexture,p):vec4(0);
    if(fillStraight)fill.rgb*=fill.a;
    float centre=silhouette(uv);
    float nearMask=0.0,farMask=0.0;
    if(edgeColour.a>0.0 && centre<0.999) {
        for(int x=-1;x<=1;x++) for(int y=-1;y<=1;y++) {
            if(x==0 && y==0)continue;
            vec2 stepUv=vec2(x,y)*pixelSize;
            nearMask=max(nearMask,silhouette(uv+stepUv*2.0));
            farMask=max(farMask,silhouette(uv+stepUv*4.0));
        }
    }
    float a=clamp((nearMask*0.72+farMask*0.22)*(1.0-centre)*edgeColour.a,0.0,1.0);
    // Fill is premultiplied; subtracting the silhouette keeps edge-only interiors untouched.
    colour=fill+vec4(edgeColour.rgb*a,a)*(1.0-fill.a);
}
)PZCOMP_FRAG";
}
