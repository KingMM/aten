#version 450

precision highp float;
precision highp int;

layout(triangles) in;
layout(triangle_strip, max_vertices = 6) out;

uniform mat4 mtxW2C;

in vec3 worldNormal[3];
in vec2 vUV[3];

out vec3 normal;
out vec2 uv;
out vec3 baryCentric;


// TODO
// �p�X�g�����̌v�Z���@�ƍ����悤�ɏ��Ԃ�Ή������Ă���̂Ŕėp���Ɋւ��Ă͗v����.

// For computing bary centric.
const vec3 weight[3] = {
	vec3(0, 0, 1),
	vec3(1, 0, 0),
	vec3(0, 1, 0),
};

void main()
{
	for (int i = 0; i < gl_in.length(); i++) {
		gl_Position = mtxW2C * gl_in[i].gl_Position;
		
		normal = worldNormal[i];
		uv = vUV[i];
		baryCentric = weight[i];

		EmitVertex();
	}
	EndPrimitive();
}
