#version 150 core
#extension GL_ARB_explicit_attrib_location : enable
#extension GL_ARB_explicit_uniform_location : enable

// �X�P�[��
uniform vec2 scale;
uniform float depthMax, depthScale;

// �e�N�X�`��
layout (location = 0) uniform sampler2D depth;

// �e�N�X�`�����W
in vec2 texcoord;

// �t���[���o�b�t�@�ɏo�͂���f�[�^
layout (location = 0) out vec3 position;

// �f�v�X�l���X�P�[�����O����
float s(in float z)
{
  return z == 0.0 ? depthMax : z * depthScale;
}

void main(void)
{
  // �f�v�X�l�����o��
  float z = s(texture(depth, texcoord).r);

  // �f�v�X�l����J�������W�l�����߂�
  position = vec3((texcoord - 0.5) * scale * z, z);
}