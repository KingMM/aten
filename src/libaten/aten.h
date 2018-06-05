#pragma once

#include "defs.h"
#include "types.h"

#include "camera/pinhole.h"
#include "camera/CameraOperator.h"

#include "filter/taa.h"

#include "material/emissive.h"
#include "material/lambert.h"
#include "material/specular.h"
#include "material/refraction.h"
#include "material/blinn.h"
#include "material/ggx.h"
#include "material/beckman.h"
#include "material/oren_nayar.h"

#include "math/math.h"
#include "math/vec3.h"
#include "math/vec4.h"
#include "math/ray.h"
#include "math/mat4.h"
#include "math/quaternion.h"
#include "math/aabb.h"

#include "misc/color.h"
#include "misc/timer.h"
#include "misc/omputil.h"
#include "misc/value.h"
#include "misc/stream.h"
#include "misc/thread.h"
#include "misc/key.h"
#include "misc/timeline.h"

#include "light/light.h"
#include "light/pointlight.h"
#include "light/directionallight.h"
#include "light/spotlight.h"
#include "light/arealight.h"
#include "light/ibl.h"

#include "proxy/DataCollector.h"

#include "texture/texture.h"

#include "hdr/hdr.h"
#include "hdr/gamma.h"

#include "visualizer/visualizer.h"
#include "visualizer/window.h"
#include "visualizer/shader.h"
#include "visualizer/blitter.h"
#include "visualizer/RasterizeRenderer.h"
#include "visualizer/fbo.h"
#include "visualizer/GLProfiler.h"

#include "scene/scene.h"
#include "scene/instance.h"

#include "accelerator/accelerator.h"
#include "accelerator/bvh.h"
#include "accelerator/sbvh.h"
#include "accelerator/threaded_bvh.h"

#include "accelerator/GpuPayloadDefs.h"

#include "sampler/xorshift.h"
#include "sampler/halton.h"
#include "sampler/sobolproxy.h"
#include "sampler/wanghash.h"

#include "geometry/object.h"
#include "geometry/objshape.h"
#include "geometry/face.h"
#include "geometry/vertex.h"
#include "geometry/sphere.h"
#include "geometry/cube.h"
#include "geometry/tranformable.h"

#include "deformable/deformable.h"
#include "deformable/DeformAnimation.h"

#include "renderer/renderer.h"
#include "renderer/film.h"
#include "renderer/background.h"
#include "renderer/envmap.h"
#include "renderer/pathtracing.h"

#include "os/system.h"