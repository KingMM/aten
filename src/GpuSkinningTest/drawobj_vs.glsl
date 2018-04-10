#version 450
precision highp float;
precision highp int;

layout(location = 0) in vec4 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec3 uv;

// NOTE
// �O���[�o���}�g���N�X�v�Z���Ƀ��[�g�� local to world �}�g���N�X�͏�Z�ς�.
// ���̂��߁A�V�F�[�_�ł͌v�Z����K�v���Ȃ��̂ŁA�V�F�[�_�ɓn����Ă��Ȃ�.

uniform mat4 mtxW2C;

layout(location = 0) out vec3 outNormal;
layout(location = 1) out vec2 outUV;

void main()
{
	gl_Position = mtxW2C * position;

	outNormal = normalize(normal);
	outUV = uv.xy;
}
