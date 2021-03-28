#ifndef PGM_IMAGE_H
#define PGM_IMAGE_H

#include <memory>
#include <cstdint>

struct pgm_image {
	pgm_image(std::string const& filename, char file_type);

	pgm_image(pgm_image const&) = delete;

	pgm_image& operator=(pgm_image const&) = delete;

	~pgm_image() = default;

	void print_to_file(std::string const& filename, char dither_type, uint8_t num_bits, double gamma);

private:
	std::unique_ptr<double[]> data;
	uint32_t w, h;
	uint16_t depth;

	void no_dither(uint8_t num_bits, double gamma);

	void ordered_dither(uint8_t num_bits, double gamma);

	void random_dither(uint8_t num_bits, double gamma);

	void floyd_steinberg_dither(uint8_t num_bits, double gamma);

	void jarvis_judice_ninke_dither(uint8_t num_bits, double gamma);

	void sierra_dither(uint8_t num_bits, double gamma);

	void atkinson_dither(uint8_t num_bits, double gamma);

	void halftone_dither(uint8_t num_bits, double gamma);
};

#endif