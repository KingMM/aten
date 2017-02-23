#include "hdr/hdr.h"

namespace aten
{
	struct HDRPixel {
		unsigned char r, g, b, e;

		HDRPixel(const unsigned char r_ = 0, const unsigned char g_ = 0, const unsigned char b_ = 0, const unsigned char e_ = 0) :
			r(r_), g(g_), b(b_), e(e_) {};

		unsigned char get(int idx) {
			switch (idx) {
			case 0: return r;
			case 1: return g;
			case 2: return b;
			case 3: return e;
			} return 0;
		}
	};

	// double��RGB�v�f��.hdr�t�H�[�}�b�g�p�ɕϊ�.
	static HDRPixel get_hdr_pixel(const vec3& color)
	{
		double d = std::max(color.x, std::max(color.y, color.z));

		if (d <= 1e-32) {
			return HDRPixel();
		}

		int e;
		double m = frexp(d, &e); // d = m * 2^e
		d = m * 256.0 / d;

		return HDRPixel(color.x * d, color.y * d, color.z * d, e + 128);
	}

	bool HDRExporter::save(
		const std::string& filename,
		const vec3* image,
		const int width, const int height)
	{
		FILE *fp;
		fopen_s(&fp, filename.c_str(), "wb");
		if (fp == NULL) {
			AT_PRINTF("Error: %s\n", filename);
			return false;
		}

		// .hdr�t�H�[�}�b�g�ɏ]���ăf�[�^����������.
		// �w�b�_.
		unsigned char ret = 0x0a;
		fprintf(fp, "#?RADIANCE%c", (unsigned char)ret);
		fprintf(fp, "# Made with 100%% pure HDR Shop%c", ret);
		fprintf(fp, "FORMAT=32-bit_rle_rgbe%c", ret);
		fprintf(fp, "EXPOSURE=1.0000000000000%c%c", ret, ret);

		// �P�x�l�����o��.
		fprintf(fp, "-Y %d +X %d%c", height, width, ret);

		for (int i = height - 1; i >= 0; i--) {
			std::vector<HDRPixel> line;
			for (int j = 0; j < width; j++) {
				HDRPixel p = get_hdr_pixel(image[j + i * width]);
				line.push_back(p);
			}

			fprintf(fp, "%c%c", 0x02, 0x02);
			fprintf(fp, "%c%c", (width >> 8) & 0xFF, width & 0xFF);

			for (int i = 0; i < 4; i++) {
				for (int cursor = 0; cursor < width;) {
					const int cursor_move = std::min(127, width - cursor);
					fprintf(fp, "%c", cursor_move);

					for (int j = cursor; j < cursor + cursor_move; j++) {
						fprintf(fp, "%c", line[j].get(i));
					}

					cursor += cursor_move;
				}
			}
		}

		fclose(fp);

		return true;
	}
}