#include "scenedefs.h"
#include "atenscene.h"

static aten::instance<aten::object>* g_movableObj = nullptr;

aten::instance<aten::object>* getMovableObj()
{
	return g_movableObj;
}


/////////////////////////////////////////////////////

void ObjCornellBoxScene::makeScene(aten::scene* scene)
{
	auto emit = new aten::emissive(aten::vec3(36, 33, 24));
	aten::AssetManager::registerMtrl(
		"light",
		emit);

	aten::AssetManager::registerMtrl(
		"backWall",
		new aten::lambert(aten::vec3(0.580000, 0.568000, 0.544000)));
	aten::AssetManager::registerMtrl(
		"ceiling",
		new aten::lambert(aten::vec3(0.580000, 0.568000, 0.544000)));
	aten::AssetManager::registerMtrl(
		"floor",
		new aten::lambert(aten::vec3(0.580000, 0.568000, 0.544000)));
	aten::AssetManager::registerMtrl(
		"leftWall",
		new aten::lambert(aten::vec3(0.504000, 0.052000, 0.040000)));

	aten::AssetManager::registerMtrl(
		"rightWall",
		new aten::lambert(aten::vec3(0.112000, 0.360000, 0.072800)));
	aten::AssetManager::registerMtrl(
		"shortBox",
		new aten::lambert(aten::vec3(0.580000, 0.568000, 0.544000)));
	aten::AssetManager::registerMtrl(
		"tallBox",
		new aten::lambert(aten::vec3(0.580000, 0.568000, 0.544000)));

	std::vector<aten::object*> objs;

#if 1
	aten::ObjLoader::load(objs, "../../asset/cornellbox/orig.obj", true);

	auto light = new aten::instance<aten::object>(
		objs[0],
		aten::vec3(0),
		aten::vec3(0),
		aten::vec3(1));
	scene->add(light);

	g_movableObj = light;

	auto areaLight = new aten::AreaLight(light, emit->param().baseColor);
	scene->addLight(areaLight);

	for (int i = 1; i < objs.size(); i++) {
		auto box = new aten::instance<aten::object>(objs[i], aten::mat4::Identity);
		scene->add(box);
	}
#else
	aten::ObjLoader::load(objs, "../../asset/cornellbox/orig_nolight.obj");
	auto box = new aten::instance<aten::object>(objs[0], aten::mat4::Identity);
	scene->add(box);
#endif
}

void ObjCornellBoxScene::getCameraPosAndAt(
	aten::vec3& pos,
	aten::vec3& at,
	real& fov)
{
	pos = aten::vec3(0.f, 1.f, 3.f);
	at = aten::vec3(0.f, 1.f, 0.f);
	fov = 45;
}

/////////////////////////////////////////////////////

void SponzaScene::makeScene(aten::scene* scene)
{
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
		new aten::lambert(aten::vec3(0.941177, 0.866667, 0.737255), KAMEN_stup));	// TODO	specularÇ™Ç†ÇÈÇÃÇ≈ÅAlambertÇ≈Ç»Ç¢.
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

	objs[0]->importInternalAccelTree("../../asset/sponza/sponza.sbvh");

	auto sponza = new aten::instance<aten::object>(objs[0], aten::mat4::Identity);

#if 1
	{
		int offsetTriIdx = aten::face::faces().size();

		objs.clear();
		aten::ObjLoader::load(objs, "../../asset/sponza/sponza_lod.obj");
		objs[0]->importInternalAccelTree("../../asset/sponza/sponza_lod.sbvh", offsetTriIdx);
		sponza->setLod(objs[0]);
	}
#endif

	scene->add(sponza);
}

void SponzaScene::getCameraPosAndAt(
	aten::vec3& pos,
	aten::vec3& at,
	real& fov)
{
	pos = aten::vec3(0.f, 1.f, 3.f);
	at = aten::vec3(0.f, 1.f, 0.f);
	fov = 45;
}
