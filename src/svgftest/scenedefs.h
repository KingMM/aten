#include "aten.h"

aten::instance<aten::object>* getMovableObj();

class ObjCornellBoxScene {
public:
	static void makeScene(aten::scene* scene);

	static void getCameraPosAndAt(
		aten::vec3& pos,
		aten::vec3& at,
		real& fov);
};

class SponzaScene {
public:
	static void makeScene(aten::scene* scene);

	static void getCameraPosAndAt(
		aten::vec3& pos,
		aten::vec3& at,
		real& fov);
};

//#define Scene ObjCornellBoxScene
#define Scene SponzaScene
