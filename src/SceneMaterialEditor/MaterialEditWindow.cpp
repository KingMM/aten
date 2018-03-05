#include "MaterialEditWindow.h"

#include "cuda_runtime.h"
#include "device_launch_parameters.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>

#include <imgui.h>

#include "atenscene.h"

aten::PinholeCamera MaterialEditWindow::s_camera;
bool MaterialEditWindow::s_isCameraDirty = false;

aten::AcceleratedScene<aten::GPUBvh> MaterialEditWindow::s_scene;

idaten::PathTracing MaterialEditWindow::s_tracer;

aten::GammaCorrection MaterialEditWindow::s_gamma;
aten::visualizer* MaterialEditWindow::s_visualizer = nullptr;

bool MaterialEditWindow::s_willShowGUI = true;
bool MaterialEditWindow::s_willTakeScreenShot = false;
int MaterialEditWindow::s_cntScreenShot = 0;

int MaterialEditWindow::s_maxSamples = 1;
int MaterialEditWindow::s_maxBounce = 5;

bool MaterialEditWindow::s_isMouseLBtnDown = false;
bool MaterialEditWindow::s_isMouseRBtnDown = false;
int MaterialEditWindow::s_prevX = 0;
int MaterialEditWindow::s_prevY = 0;

int MaterialEditWindow::s_width = 0;
int MaterialEditWindow::s_height = 0;

void getCameraPosAndAt(
	aten::vec3& pos,
	aten::vec3& at,
	real& fov)
{
	pos = aten::vec3(0.f, 1.f, 10.f);
	at = aten::vec3(0.f, 1.f, 0.f);
	fov = 45;
}

void makeScene(aten::scene* scene)
{
	aten::AssetManager::registerMtrl(
		"m1",
		new aten::lambert(aten::vec3(0.580000, 0.580000, 0.580000)));

	auto obj = aten::ObjLoader::load("../../asset/teapot/teapot.obj");
	auto teapot = new aten::instance<aten::object>(obj, aten::mat4::Identity);
	scene->add(teapot);
}

aten::material* createMaterial(aten::MaterialType type)
{
	aten::material* mtrl = nullptr;

	switch (type) {
	case aten::MaterialType::Emissive:
	case aten::MaterialType::Lambert:
		mtrl = new aten::lambert();
		break;
	case aten::MaterialType::OrneNayar:
		mtrl = new aten::OrenNayar();
		break;
	case aten::MaterialType::Specular:
		mtrl = new aten::specular();
		break;
	case aten::MaterialType::Refraction:
		mtrl = new aten::refraction();
		break;
	case aten::MaterialType::Blinn:
		mtrl = new aten::MicrofacetBlinn();
		break;
	case aten::MaterialType::GGX:
		mtrl = new aten::MicrofacetGGX();
		break;
	case aten::MaterialType::Beckman:
		mtrl = new aten::MicrofacetBeckman();
		break;
	default:
		AT_ASSERT(false);
		mtrl = new aten::lambert();
		break;
	}

	return mtrl;
}

class MaterialParamEditor : public aten::IMaterialParamEditor {
public:
	MaterialParamEditor() {}
	virtual ~MaterialParamEditor() {}

public:
	virtual bool edit(const char* name, real& param, real _min = real(0), real _max = real(1)) override final
	{
		return ImGui::SliderFloat(name, &param, _min, _max);
	}

	virtual bool edit(const char* name, aten::vec3& param) override final
	{
		float f[3] = { param.x, param.y, param.z };
		bool ret = ImGui::ColorEdit3(name, f);

		param.x = f[0];
		param.y = f[1];
		param.z = f[2];

		return ret;
	}
};

MaterialParamEditor s_mtrlParamEditor;

void MaterialEditWindow::onRun(aten::window* window)
{
	float updateTime = 0.0f;

	if (s_isCameraDirty) {
		s_camera.update();

		auto camparam = s_camera.param();
		camparam.znear = real(0.1);
		camparam.zfar = real(10000.0);

		s_tracer.updateCamera(camparam);
		s_isCameraDirty = false;

		s_visualizer->clear();
	}

	s_tracer.render(
		s_width, s_height,
		s_maxSamples,
		s_maxBounce);


	s_visualizer->render(false);

	if (s_willTakeScreenShot)
	{
		static char buffer[1024];
		::sprintf(buffer, "sc_%d.png\0", s_cntScreenShot);

		s_visualizer->takeScreenshot(buffer);

		s_willTakeScreenShot = false;
		s_cntScreenShot++;

		AT_PRINTF("Take Screenshot[%s]\n", buffer);
	}

	if (s_willShowGUI)
	{
		if (ImGui::SliderInt("Samples", &s_maxSamples, 1, 100)
			|| ImGui::SliderInt("Bounce", &s_maxBounce, 1, 10))
		{
			s_tracer.reset();
		}

		bool isProgressive = s_tracer.isProgressive();

		if (ImGui::Checkbox("Progressive", &isProgressive)) {
			s_tracer.enableProgressive(isProgressive);
			s_tracer.reset();
		}

		auto mtrl = aten::material::getMaterial(0);
		bool needUpdateMtrl = false;

		static const char* items[] = {
			"Emissive",
			"Lambert",
			"OrneNayar",
			"Specular",
			"Refraction",
			"Blinn",
			"GGX",
			"Beckman",
		};
		int mtrlType = (int)mtrl->param().type;
		if (ImGui::Combo("mode", &mtrlType, items, AT_COUNTOF(items))) {
			aten::material::deleteMaterial(mtrl, true);
			mtrl = createMaterial((aten::MaterialType)mtrlType);
			needUpdateMtrl = true;
		}

		if (mtrl->edit(&s_mtrlParamEditor)) {
			needUpdateMtrl = true;
		}

		if (needUpdateMtrl) {
			std::vector<aten::MaterialParameter> params(1);
			params[0] = mtrl->param();
			s_tracer.updateMaterial(params);
			s_tracer.reset();
		}

		window->drawImGui();
	}
}

void MaterialEditWindow::onClose()
{

}

void MaterialEditWindow::onMouseBtn(bool left, bool press, int x, int y)
{
	s_isMouseLBtnDown = false;
	s_isMouseRBtnDown = false;

	if (press) {
		s_prevX = x;
		s_prevY = y;

		s_isMouseLBtnDown = left;
		s_isMouseRBtnDown = !left;
	}
}

void MaterialEditWindow::onMouseMove(int x, int y)
{
	if (s_isMouseLBtnDown) {
		aten::CameraOperator::rotate(
			s_camera,
			s_width, s_height,
			s_prevX, s_prevY,
			x, y);
		s_isCameraDirty = true;
	}
	else if (s_isMouseRBtnDown) {
		aten::CameraOperator::move(
			s_camera,
			s_prevX, s_prevY,
			x, y,
			real(0.001));
		s_isCameraDirty = true;
	}

	s_prevX = x;
	s_prevY = y;
}

void MaterialEditWindow::onMouseWheel(int delta)
{
	aten::CameraOperator::dolly(s_camera, delta * real(0.1));
	s_isCameraDirty = true;
}

void MaterialEditWindow::onKey(bool press, aten::Key key)
{
	static const real offset = real(0.1);

	if (press) {
		if (key == aten::Key::Key_F1) {
			s_willShowGUI = !s_willShowGUI;
			return;
		}
		else if (key == aten::Key::Key_F2) {
			s_willTakeScreenShot = true;
			return;
		}
	}

	if (press) {
		switch (key) {
		case aten::Key::Key_W:
		case aten::Key::Key_UP:
			aten::CameraOperator::moveForward(s_camera, offset);
			break;
		case aten::Key::Key_S:
		case aten::Key::Key_DOWN:
			aten::CameraOperator::moveForward(s_camera, -offset);
			break;
		case aten::Key::Key_D:
		case aten::Key::Key_RIGHT:
			aten::CameraOperator::moveRight(s_camera, offset);
			break;
		case aten::Key::Key_A:
		case aten::Key::Key_LEFT:
			aten::CameraOperator::moveRight(s_camera, -offset);
			break;
		case aten::Key::Key_Z:
			aten::CameraOperator::moveUp(s_camera, offset);
			break;
		case aten::Key::Key_X:
			aten::CameraOperator::moveUp(s_camera, -offset);
			break;
		case aten::Key::Key_R:
		{
			aten::vec3 pos, at;
			real vfov;
			getCameraPosAndAt(pos, at, vfov);

			s_camera.init(
				pos,
				at,
				aten::vec3(0, 1, 0),
				vfov,
#ifdef ENABLE_GEOMRENDERING
				WIDTH >> 1, HEIGHT >> 1);
#else
				s_width, s_height);
#endif
		}
			break;
		default:
			break;
		}

		s_isCameraDirty = true;
	}
}

bool MaterialEditWindow::init(
	int width, int height,
	const char* title)
{
	s_width = width;
	s_height = height;

	aten::initSampler(s_width, s_height);

	auto wnd = aten::window::init(
		s_width, s_height, title,
		MaterialEditWindow::onRun,
		MaterialEditWindow::onClose,
		MaterialEditWindow::onMouseBtn,
		MaterialEditWindow::onMouseMove,
		MaterialEditWindow::onMouseWheel,
		MaterialEditWindow::onKey);

	wnd->asCurrent();

	s_visualizer = aten::visualizer::init(s_width, s_height);
	
	s_gamma.init(
		s_width, s_height,
		"../shader/fullscreen_vs.glsl",
		"../shader/gamma_fs.glsl");

	s_visualizer->addPostProc(&s_gamma);

	aten::vec3 pos, at;
	real vfov;
	getCameraPosAndAt(pos, at, vfov);

	s_camera.init(
		pos,
		at,
		aten::vec3(0, 1, 0),
		vfov,
		s_width, s_height);

	makeScene(&s_scene);
	s_scene.build();

	idaten::Compaction::init(
		s_width * s_height,
		1024);

	auto envmap = aten::ImageLoader::load("../../asset/envmap/studio015.hdr");
	aten::envmap bg;
	bg.init(envmap);
	aten::ImageBasedLight ibl(&bg);

	s_scene.addImageBasedLight(&ibl);

	{
		std::vector<aten::GeomParameter> shapeparams;
		std::vector<aten::PrimitiveParamter> primparams;
		std::vector<aten::LightParameter> lightparams;
		std::vector<aten::MaterialParameter> mtrlparms;
		std::vector<aten::vertex> vtxparams;

		aten::DataCollector::collect(
			shapeparams,
			primparams,
			lightparams,
			mtrlparms,
			vtxparams);

		const auto& nodes = s_scene.getAccel()->getNodes();
		const auto& mtxs = s_scene.getAccel()->getMatrices();

		std::vector<idaten::TextureResource> tex;
		{
			auto texs = aten::texture::getTextures();

			for (const auto t : texs) {
				tex.push_back(
					idaten::TextureResource(t->colors(), t->width(), t->height()));
			}
		}

		for (auto& l : lightparams) {
			if (l.type == aten::LightType::IBL) {
				l.envmap.idx = envmap->id();
			}
		}

		auto camparam = s_camera.param();
		camparam.znear = real(0.1);
		camparam.zfar = real(10000.0);

		s_tracer.update(
			aten::visualizer::getTexHandle(),
			s_width, s_height,
			camparam,
			shapeparams,
			mtrlparms,
			lightparams,
			nodes,
			primparams,
			vtxparams,
			mtxs,
			tex,
			idaten::EnvmapResource(envmap->id(), ibl.getAvgIlluminace(), real(1)));
	}

	return true;
}