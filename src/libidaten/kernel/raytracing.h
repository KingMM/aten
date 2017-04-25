#pragma once

#include "aten4idaten.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

	void prepareRayTracing();

	void renderRayTracing(
		aten::vec4* image,
		int width, int height,
		const aten::CameraParameter& camera,
		const std::vector<aten::ShapeParameter>& shapes,
		const std::vector<aten::MaterialParameter>& mtrls,
		const std::vector<aten::LightParameter>& lights);

#ifdef __cplusplus
}
#endif /* __cplusplus */
