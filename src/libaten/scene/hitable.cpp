#include <atomic>
#include "scene/hitable.h"

namespace aten
{
	// NOTE
	// 0 �͗\��ς݂Ȃ̂ŁA1 ����n�߂�.
	static std::atomic<uint32_t> g_id = 0;

	hitable::hitable(const char* name/*= nullptr*/)
		: m_name(name)
	{
		m_id = g_id.fetch_add(1);
	}
}
