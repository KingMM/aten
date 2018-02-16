#pragma once

#include <vector>
#include <map>

class FbxDataManager;

namespace aten
{
	class FbxImporter {
		friend class aten::FbxImporter;

	public:
		FbxImporter();
		~FbxImporter() { close(); }

	public:
		bool open(const char* pszName, bool isOpenForAnm = false);
		bool close();

		//////////////////////////////////
		// For geometry chunk.

		void exportGeometryCompleted();

		uint32_t getMeshNum();

		// ���b�V���Ɋւ��鏈�����J�n.
		void beginMesh(uint32_t nIdx);

		// ���b�V���Ɋւ��鏈�����I��.
		void endMesh();

		// BeginMesh�Ŏw�肳�ꂽ���b�V���Ɋ܂܂�X�L�j���O�����擾.
		void getSkinList(std::vector<SkinParam>& tvSkinList);

		// BeginMesh�Ŏw�肳�ꂽ���b�V���Ɋ܂܂��O�p�`���擾.
		uint32_t getTriangles(std::vector<TriangleParam>& tvTriList);

		// �w�肳�ꂽ���_�ɉe����^����X�L�j���O���ւ̃C���f�b�N�X���擾.
		uint32_t getSkinIdxAffectToVtx(uint32_t nVtxIdx);

		// �P���_������̃T�C�Y���擾.
		// �������A�X�L�j���O�Ɋւ���T�C�Y�͊܂܂Ȃ�
		uint32_t getVtxSize();

		// ���_�t�H�[�}�b�g���擾.
		// �������A�X�L�j���O�Ɋւ���t�H�[�}�b�g�͊܂܂Ȃ�
		uint32_t getVtxFmt();

		// �w�肳�ꂽ���_�ɂ�����w��t�H�[�}�b�g�̃f�[�^���擾.
		bool getVertex(
			uint32_t nIdx,
			izanagi::math::SVector4& vec,
			izanagi::E_MSH_VTX_FMT_TYPE type);

		void getMaterialForMesh(
			uint32_t nMeshIdx,
			izanagi::S_MSH_MTRL& sMtrl);

		//////////////////////////////////
		// For joint chunk.

		// �֐߃f�[�^�̏o�͊�����ʒm.
		void exportJointCompleted();

		// �֐߂Ɋւ��鏈�����J�n.
		bool beginJoint();

		// �֐߂Ɋւ��鏈�����I��.
		void endJoint();

		// �֐ߐ����擾.
		uint32_t getJointNum();

		// �w�肳�ꂽ�֐߂̖��O���擾.
		const char* GetJointName(uint32_t nIdx);

		// �e�֐߂ւ̃C���f�b�N�X���擾.    
		int32_t getJointParent(
			uint32_t nIdx,
			const std::vector<izanagi::S_SKL_JOINT>& tvJoint);

		// �w�肳�ꂽ�֐߂̋t�}�g���N�X���擾.  
		void getJointInvMtx(
			uint32_t nIdx,
			izanagi::math::SMatrix44& mtx);

		// �֐߂̎p�����擾.
		void getJointTransform(
			uint32_t nIdx,
			const std::vector<izanagi::S_SKL_JOINT>& tvJoint,
			std::vector<JointTransformParam>& tvTransform);

		//////////////////////////////////
		// For animation.

		// ���[�V�����̑ΏۂƂȂ郂�f���f�[�^���w��.
		bool readBaseModel(const char* pszName);

		// �t�@�C���Ɋ܂܂�郂�[�V�����̐����擾.
		uint32_t getAnmSetNum();

		// ���[�V�����Ɋւ��鏈�����J�n.
		bool beginAnm(uint32_t nSetIdx);

		// ���[�V�����Ɋւ��鏈�����I��.
		bool endAnm();

		// ���[�V�����m�[�h�i�K�p�W���C���g�j�̐����擾.
		uint32_t getAnmNodeNum();

		// �A�j���[�V�����`�����l���̐����擾.
		// �A�j���[�V�����`�����l���Ƃ�
		// �W���C���g�̃p�����[�^�iex. �ʒu�A��]�Ȃǁj���Ƃ̃A�j���[�V�������̂���
		uint32_t getAnmChannelNum(uint32_t nNodeIdx);

		// ���[�V�����m�[�h�i�K�p�W���C���g�j�̏����擾.
		bool getAnmNode(
			uint32_t nNodeIdx,
			izanagi::S_ANM_NODE& sNode);

		// �A�j���[�V�����`�����l���̏����擾.
		// �A�j���[�V�����`�����l���Ƃ�
		// �W���C���g�̃p�����[�^�iex. �ʒu�A��]�Ȃǁj���Ƃ̃A�j���[�V�������̂���
		bool getAnmChannel(
			uint32_t nNodeIdx,
			uint32_t nChannelIdx,
			izanagi::S_ANM_CHANNEL& sChannel);

		// �L�[�t���[�������擾.
		// �L�[�t���[��������̃W���C���g�̃p�����[�^�ɓK�p����p�����[�^���擾.
		bool getAnmKey(
			uint32_t nNodeIdx,
			uint32_t nChannelIdx,
			uint32_t nKeyIdx,
			izanagi::S_ANM_KEY& sKey,
			std::vector<float>& tvValue);

		//////////////////////////////////
		// For material.

		bool beginMaterial();

		bool endMaterial();

		uint32_t getMaterialNum();

		bool getMaterial(
			uint32_t nMtrlIdx,
			izanagi::S_MTRL_MATERIAL& sMtrl);

		void getMaterialTexture(
			uint32_t nMtrlIdx,
			uint32_t nTexIdx,
			izanagi::S_MTRL_TEXTURE& sTex);

		void getMaterialShader(
			uint32_t nMtrlIdx,
			uint32_t nShaderIdx,
			izanagi::S_MTRL_SHADER& sShader);

		void getMaterialParam(
			uint32_t nMtrlIdx,
			uint32_t nParamIdx,
			izanagi::S_MTRL_PARAM& sParam);

		void getMaterialParamValue(
			uint32_t nMtrlIdx,
			uint32_t nParamIdx,
			std::vector<float>& tvValue);

	private:
		bool getFbxMatrial(
			uint32_t nMtrlIdx,
			izanagi::S_MTRL_MATERIAL& sMtrl);

		bool getFbxMatrialByImplmentation(
			uint32_t nMtrlIdx,
			izanagi::S_MTRL_MATERIAL& sMtrl);

	private:
		FbxDataManager* m_dataMgr{ nullptr };
		FbxDataManager* m_dataMgrBase{ nullptr };

		uint32_t m_curMeshIdx{ 0 };
		uint32_t m_posVtx{ 0 };

		uint32_t m_curAnmIdx{ 0 };

		struct MaterialTex {
			void* fbxMtrl{ nullptr };
			std::string paramName;
			std::string texName;
			izanagi::S_MTRL_TEXTURE_TYPE type;
		};
		std::map<uint32_t, std::vector<MaterialTex>> m_mtrlTex;

		struct MaterialShading {
			void* fbxMtrl{ nullptr };
			std::string name;
		};
		std::map<uint32_t, std::vector<MaterialShading>> m_mtrlShd;

		struct MaterialParam {
			void* fbxMtrl{ nullptr };
			std::string name;
			std::vector<float> values;
		};
		std::map<uint32_t, std::vector<MaterialParam>> m_mtrlParam;

		void getLambertParams(void* mtrl, std::vector<MaterialParam>& list);
		void getPhongParams(void* mtrl, std::vector<MaterialParam>& list);

		enum ParamType {
			Tranlate,
			Scale,
			Rotate,

			Num,
		};

		struct AnmKey {
			uint32_t key;
			float value[4];

			AnmKey() {}
		};

		struct AnmChannel {
			uint32_t nodeIdx;
			ParamType type[ParamType::Num];

			std::vector<AnmKey> keys[ParamType::Num];

			bool isChecked{ false };

			AnmChannel()
			{
				for (uint32_t i = 0; i < ParamType::Num; i++) {
					type[i] = ParamType::Num;
				}
			}
		};
		std::vector<AnmChannel> m_channels;
	};
}