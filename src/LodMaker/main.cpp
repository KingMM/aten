#include "aten.h"
#include "atenscene.h"

#include "lodmaker.h"

#include <cmdline.h>

static const int WIDTH = 1280;
static const int HEIGHT = 720;

static const char* TITLE = "LodMaker";

static aten::RasterizeRenderer g_rasterizer;

static aten::object* g_obj = nullptr;

static aten::PinholeCamera g_camera;
static bool g_isCameraDirty = false;

static bool g_willShowGUI = true;
static bool g_willTakeScreenShot = false;
static int g_cntScreenShot = 0;

static bool g_isMouseLBtnDown = false;
static bool g_isMouseRBtnDown = false;
static int g_prevX = 0;
static int g_prevY = 0;

void onRun()
{
	if (g_isCameraDirty) {
		g_camera.update();

		auto camparam = g_camera.param();
		camparam.znear = real(0.1);
		camparam.zfar = real(10000.0);

		g_isCameraDirty = false;
	}

	g_rasterizer.draw(g_obj, &g_camera);
}

void onClose()
{

}

void onMouseBtn(bool left, bool press, int x, int y)
{
	g_isMouseLBtnDown = false;
	g_isMouseRBtnDown = false;

	if (press) {
		g_prevX = x;
		g_prevY = y;

		g_isMouseLBtnDown = left;
		g_isMouseRBtnDown = !left;
	}
}

void onMouseMove(int x, int y)
{
	if (g_isMouseLBtnDown) {
		aten::CameraOperator::rotate(
			g_camera,
			WIDTH, HEIGHT,
			g_prevX, g_prevY,
			x, y);
		g_isCameraDirty = true;
	}
	else if (g_isMouseRBtnDown) {
		aten::CameraOperator::move(
			g_camera,
			g_prevX, g_prevY,
			x, y,
			real(0.001));
		g_isCameraDirty = true;
	}

	g_prevX = x;
	g_prevY = y;
}

void onMouseWheel(int delta)
{
	aten::CameraOperator::dolly(g_camera, delta * real(0.1));
	g_isCameraDirty = true;
}

void onKey(bool press, aten::Key key)
{
	static const real offset = real(0.1);

	if (press) {
		if (key == aten::Key::Key_F1) {
			g_willShowGUI = !g_willShowGUI;
			return;
		}
		else if (key == aten::Key::Key_F2) {
			g_willTakeScreenShot = true;
			return;
		}
	}

	if (press) {
		switch (key) {
		case aten::Key::Key_W:
		case aten::Key::Key_UP:
			aten::CameraOperator::moveForward(g_camera, offset);
			break;
		case aten::Key::Key_S:
		case aten::Key::Key_DOWN:
			aten::CameraOperator::moveForward(g_camera, -offset);
			break;
		case aten::Key::Key_D:
		case aten::Key::Key_RIGHT:
			aten::CameraOperator::moveRight(g_camera, offset);
			break;
		case aten::Key::Key_A:
		case aten::Key::Key_LEFT:
			aten::CameraOperator::moveRight(g_camera, -offset);
			break;
		case aten::Key::Key_Z:
			aten::CameraOperator::moveUp(g_camera, offset);
			break;
		case aten::Key::Key_X:
			aten::CameraOperator::moveUp(g_camera, -offset);
			break;
		default:
			break;
		}

		g_isCameraDirty = true;
	}
}

struct Options {
	std::string input;
	std::string output;
};

bool parseOption(
	int argc, char* argv[],
	cmdline::parser& cmd,
	Options& opt)
{
	{
		cmd.add<std::string>("input", 'i', "input filename", true);
		cmd.add<std::string>("output", 'o', "output filename base", false, "result");

		cmd.add<std::string>("help", '?', "print usage", false);
	}

	bool isCmdOk = cmd.parse(argc, argv);

	if (cmd.exist("help")) {
		std::cerr << cmd.usage();
		return false;
	}

	if (!isCmdOk) {
		std::cerr << cmd.error() << std::endl << cmd.usage();
		return false;
	}

	if (cmd.exist("input")) {
		opt.input = cmd.get<std::string>("input");
	}
	else {
		std::cerr << cmd.error() << std::endl << cmd.usage();
		return false;
	}

	if (cmd.exist("output")) {
		opt.output = cmd.get<std::string>("output");
	}
	else {
		// TODO
		opt.output = "result.sbvh";
	}

	return true;
}

// TODO
aten::object* loadObj()
{
#if 0
	auto SP_LUK = aten::ImageLoader::load("../../asset/sponza/sp_luk.JPG");
	auto SP_LUK_nml = aten::ImageLoader::load("../../asset/sponza/sp_luk-nml.png");

	auto _00_SKAP = aten::ImageLoader::load("../../asset/sponza/00_skap.JPG");

	auto _01_STUB = aten::ImageLoader::load("../../asset/sponza/01_STUB.JPG");
	auto _01_STUB_nml = aten::ImageLoader::load("../../asset/sponza/01_STUB-nml.png");

	auto _01_S_BA = aten::ImageLoader::load("../../asset/sponza/01_S_ba.JPG");

	auto _01_ST_KP = aten::ImageLoader::load("../../asset/sponza/01_St_kp.JPG");
	auto _01_ST_KP_nml = aten::ImageLoader::load("../../asset/sponza/01_St_kp-nml.png");

	auto X01_ST = aten::ImageLoader::load("../../asset/sponza/x01_st.JPG");

	auto KAMEN_stup = aten::ImageLoader::load("../../asset/sponza/KAMEN-stup.JPG");

	auto RELJEF = aten::ImageLoader::load("../../asset/sponza/reljef.JPG");
	auto RELJEF_nml = aten::ImageLoader::load("../../asset/sponza/reljef-nml.png");

	auto KAMEN = aten::ImageLoader::load("../../asset/sponza/KAMEN.JPG");
	auto KAMEN_nml = aten::ImageLoader::load("../../asset/sponza/KAMEN-nml.png");

	auto PROZOR1 = aten::ImageLoader::load("../../asset/sponza/prozor1.JPG");

	auto VRATA_KR = aten::ImageLoader::load("../../asset/sponza/vrata_kr.JPG");

	auto VRATA_KO = aten::ImageLoader::load("../../asset/sponza/vrata_ko.JPG");

	std::vector<aten::texture*> albedo;
	albedo.push_back(SP_LUK);
	albedo.push_back(_00_SKAP);
	albedo.push_back(_01_STUB);
	albedo.push_back(_01_S_BA);
	albedo.push_back(_01_ST_KP);
	albedo.push_back(X01_ST);
	albedo.push_back(KAMEN_stup);
	albedo.push_back(RELJEF);
	albedo.push_back(KAMEN);
	albedo.push_back(PROZOR1);
	albedo.push_back(VRATA_KR);
	albedo.push_back(VRATA_KO);

	for (int i = 0; i < albedo.size(); i++) {
		albedo[i]->initAsGLTexture();
	}

	aten::AssetManager::registerMtrl(
		"sp_00_luk_mali",
		new aten::lambert(aten::vec3(0.745098, 0.709804, 0.674510), SP_LUK, SP_LUK_nml));
	aten::AssetManager::registerMtrl(
		"sp_svod_kapitel",
		new aten::lambert(aten::vec3(0.713726, 0.705882, 0.658824), _00_SKAP));
	aten::AssetManager::registerMtrl(
		"sp_01_stub_baza_",
		new aten::lambert(aten::vec3(0.784314, 0.784314, 0.784314)));
	aten::AssetManager::registerMtrl(
		"sp_01_stub_kut",
		new aten::lambert(aten::vec3(0.737255, 0.709804, 0.670588), _01_STUB, _01_STUB_nml));
	aten::AssetManager::registerMtrl(
		"sp_00_stup",
		new aten::lambert(aten::vec3(0.737255, 0.709804, 0.670588), _01_STUB, _01_STUB_nml));
	aten::AssetManager::registerMtrl(
		"sp_01_stub_baza",
		new aten::lambert(aten::vec3(0.800000, 0.784314, 0.749020), _01_S_BA));
	aten::AssetManager::registerMtrl(
		"sp_00_luk_mal1",
		new aten::lambert(aten::vec3(0.745098, 0.709804, 0.674510), _01_ST_KP, _01_ST_KP_nml));
	aten::AssetManager::registerMtrl(
		"sp_01_stub",
		new aten::lambert(aten::vec3(0.737255, 0.709804, 0.670588), _01_STUB, _01_STUB_nml));
	aten::AssetManager::registerMtrl(
		"sp_01_stup",
		new aten::lambert(aten::vec3(0.827451, 0.800000, 0.768628), X01_ST));
	aten::AssetManager::registerMtrl(
		"sp_vijenac",
		new aten::lambert(aten::vec3(0.713726, 0.705882, 0.658824), _00_SKAP));
	aten::AssetManager::registerMtrl(
		"sp_00_svod",
		new aten::lambert(aten::vec3(0.941177, 0.866667, 0.737255), KAMEN_stup));	// TODO	specularがあるので、lambertでない.
	aten::AssetManager::registerMtrl(
		"sp_02_reljef",
		new aten::lambert(aten::vec3(0.529412, 0.498039, 0.490196), RELJEF, RELJEF_nml));
	aten::AssetManager::registerMtrl(
		"sp_01_luk_a",
		new aten::lambert(aten::vec3(0.745098, 0.709804, 0.674510), SP_LUK, SP_LUK_nml));
	aten::AssetManager::registerMtrl(
		"sp_zid_vani",
		new aten::lambert(aten::vec3(0.627451, 0.572549, 0.560784), KAMEN, KAMEN_nml));
	aten::AssetManager::registerMtrl(
		"sp_01_stup_baza",
		new aten::lambert(aten::vec3(0.800000, 0.784314, 0.749020), _01_S_BA));
	aten::AssetManager::registerMtrl(
		"sp_00_zid",
		new aten::lambert(aten::vec3(0.627451, 0.572549, 0.560784), KAMEN, KAMEN_nml));
	aten::AssetManager::registerMtrl(
		"sp_00_prozor",
		new aten::lambert(aten::vec3(1.000000, 1.000000, 1.000000), PROZOR1));
	aten::AssetManager::registerMtrl(
		"sp_00_vrata_krug",
		new aten::lambert(aten::vec3(0.784314, 0.784314, 0.784314), VRATA_KR));
	aten::AssetManager::registerMtrl(
		"sp_00_pod",
		new aten::lambert(aten::vec3(0.627451, 0.572549, 0.560784), KAMEN, KAMEN_nml));
	aten::AssetManager::registerMtrl(
		"sp_00_vrata_kock",
		new aten::lambert(aten::vec3(0.784314, 0.784314, 0.784314), VRATA_KO));

	std::vector<aten::object*> objs;

	aten::ObjLoader::load(objs, "../../asset/sponza/sponza.obj");
#else
	std::vector<aten::object*> objs;

	aten::AssetManager::registerMtrl(
		"Material.001",
		new aten::lambert(aten::vec3(0.2, 0.2, 0.7)));

	aten::ObjLoader::load(objs, "../../asset/suzanne/suzanne.obj");
#endif

	// NOTE
	// １つしかゆるさない.
	AT_ASSERT(objs.size() == 1);

	auto obj = objs[0];

	return obj;
}

int main(int argc, char* argv[])
{
	Options opt;

	// TODO
#if 0
	cmdline::parser cmd;

	if (!parseOption(argc, argv, cmd, opt)) {
		return 0;
	}
#endif

	aten::window::SetCurrentDirectoryFromExe();

	aten::AssetManager::suppressWarnings();

	aten::window::init(
		WIDTH, HEIGHT,
		TITLE,
		onClose,
		onMouseBtn,
		onMouseMove,
		onMouseWheel,
		onKey);

	g_obj = loadObj();
	g_obj->buildForRasterizeRendering();

	auto& vtxs = aten::VertexManager::getVertices();

	std::vector<std::vector<aten::face*>> tris;
	g_obj->gatherTriangles(tris);

	std::vector<aten::vertex> lodVtx;
	std::vector<std::vector<int>> lodIdx;

	LodMaker::make(
		lodVtx, lodIdx,
		g_obj->getBoundingbox(),
		vtxs, tris,
		128, 128, 128);

	// TODO
	aten::vec3 pos(0, 1, 10);
	aten::vec3 at(0, 1, 1);
	real vfov = real(45);

	g_camera.init(
		pos,
		at,
		aten::vec3(0, 1, 0),
		vfov,
		WIDTH, HEIGHT);

	g_rasterizer.init(
		WIDTH, HEIGHT,
		"../shader/drawobj_vs.glsl",
		"../shader/drawobj_fs.glsl");

	aten::window::run(onRun);

	aten::window::terminate();

	return 1;
}
