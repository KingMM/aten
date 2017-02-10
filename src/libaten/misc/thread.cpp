#include "misc/thread.h"
#include "math/math.h"

namespace aten {
	uint32_t thread::g_threadnum = 1;
	
	void thread::setThreadNum(uint32_t num)
	{
		g_threadnum = aten::clamp<uint32_t>(num, 1, 8);

#ifdef ENABLE_OMP
		omp_set_num_threads(g_threadnum);
#endif
	}

	int thread::getThreadIdx()
	{
		int idx = 0;
#ifdef ENABLE_OMP
		idx = omp_get_thread_num();
#endif
		return idx;
	}
}