#include <vector>
#include "aten.h"
#include "atenscene.h"
#include "scenedefs.h"

static int WIDTH = 512;
static int HEIGHT = 512;
static const char* TITLE = "app";

//#define ENABLE_EVERY_FRAME_SC

static aten::PinholeCamera g_camera;

static aten::AcceleratedScene<aten::ThreadedBVH> g_scene;

static aten::StaticColorBG g_staticbg(aten::vec3(0.25, 0.25, 0.25));
static aten::envmap g_bg;
static aten::texture* g_envmap;

static aten::PathTracing g_tracer;

static aten::visualizer* g_visualizer;

static aten::FilmProgressive g_buffer(WIDTH, HEIGHT);
//static aten::Film g_buffer(WIDTH, HEIGHT);

static aten::RasterizeRenderer g_rasterizerAABB;

static bool isExportedHdr = false;

#ifdef ENABLE_OMP
static uint32_t g_threadnum = 8;
#else
static uint32_t g_threadnum = 1;
#endif

static uint32_t g_frameNo = 0;
static float g_avgElapsed = 0.0f;

void update()
{
	static float y = 0.0f;
	static float d = -0.1f;

	auto obj = getMovableObj();

	if (obj) {
		auto t = obj->getTrans();

		if (y >= 0.0f) {
			d = -0.1f;
		}
		else if (y <= -0.5f) {
			d = 0.1f;

		}

		y += d;
		t.y += d;

		obj->setTrans(t);
		obj->update();

		auto accel = g_scene.getAccel();
		accel->update();
	}
}

void display(aten::window* wnd)
{
	update();

	g_camera.update();

	aten::Destination dst;
	{
		dst.width = WIDTH;
		dst.height = HEIGHT;
		dst.maxDepth = 5;
		dst.russianRouletteDepth = 3;
		dst.startDepth = 0;
		dst.sample = 1;
		dst.mutation = 10;
		dst.mltNum = 10;
		dst.buffer = &g_buffer;
	}

	dst.geominfo.albedo_vis = &g_buffer;
	dst.geominfo.depthMax = 1000;

	aten::timer timer;
	timer.begin();

	// Trace rays.
	g_tracer.render(dst, &g_scene, &g_camera);

	auto elapsed = timer.end();

	g_avgElapsed = g_avgElapsed * g_frameNo + elapsed;
	g_avgElapsed /= (g_frameNo + 1);

	AT_PRINTF("Elapsed %f[ms] / Avg %f[ms]\n", elapsed, g_avgElapsed);

	if (!isExportedHdr) {
		isExportedHdr = true;

		// Export to hdr format.
		aten::HDRExporter::save(
			"result.hdr",
			g_buffer.image(),
			WIDTH, HEIGHT);
	}

	g_visualizer->render(g_buffer.image(), g_camera.needRevert());

#if 0
	g_rasterizerAABB.drawAABB(
		&g_camera,
		g_scene.getAccel());
#endif

#ifdef ENABLE_EVERY_FRAME_SC
	{
		static char tmp[1024];
		sprintf(tmp, "sc_%d.png\0", g_frameNo);

		g_visualizer->takeScreenshot(tmp);
	}
#endif
	g_frameNo++;
}

int main(int argc, char* argv[])
{
	aten::initSampler(WIDTH, HEIGHT, 0, true);

	aten::timer::init();
	aten::OMPUtil::setThreadNum(g_threadnum);

	aten::window::init(WIDTH, HEIGHT, TITLE, display);

	g_visualizer = aten::visualizer::init(WIDTH, HEIGHT);

	aten::GammaCorrection gamma;
	gamma.init(
		WIDTH, HEIGHT,
		"../shader/fullscreen_vs.glsl",
		"../shader/gamma_fs.glsl");

	g_visualizer->addPostProc(&gamma);

	g_rasterizerAABB.init(
		WIDTH, HEIGHT,
		"../shader/simple3d_vs.glsl",
		"../shader/simple3d_fs.glsl");

	aten::vec3 lookfrom;
	aten::vec3 lookat;
	real fov;

	Scene::getCameraPosAndAt(lookfrom, lookat, fov);

	g_camera.init(
		lookfrom,
		lookat,
		aten::vec3(0, 1, 0),
		fov,
		WIDTH, HEIGHT);

	Scene::makeScene(&g_scene);

	g_scene.build();
	//g_scene.getAccel()->computeVoxelLodErrorMetric(HEIGHT, fov, 4);

	g_envmap = aten::ImageLoader::load("../../asset/envmap/studio015.hdr");
	//g_envmap = aten::ImageLoader::load("../../asset/envmap/harbor.hdr");
	g_bg.init(g_envmap);

	aten::ImageBasedLight ibl(&g_bg);
	g_scene.addImageBasedLight(&ibl);

	aten::window::run();

	aten::window::terminate();
}
