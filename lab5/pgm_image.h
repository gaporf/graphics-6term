#ifndef PGM_IMAGE_H
#define PGM_IMAGE_H

#include <memory>
#include <cstdint>
#include <string>

struct pgm_image {
	pgm_image(std::string const& filename);

	void divide_into_classes(uint32_t const classes);

	void print_to_file(std::string const& filename);

private:
	std::unique_ptr<uint8_t[]> data;
	uint32_t w, h;
	uint16_t depth;
};

#endif