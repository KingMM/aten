#version 330
precision highp float;
precision highp int;

uniform sampler2D image;

uniform vec4 invScreen;

void main()
{
    vec2 uv = gl_FragCoord.xy * invScreen.xy;
    //uv.y = 1.0 - uv.y;

    vec4 color = texture2D(image, uv);

    gl_FragColor = color;
}