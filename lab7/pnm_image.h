#ifndef PNM_IMAGE_H
#define PNM_IMAGE_H

#include <vector>
#include <cstdint>
#include <string>
#include <fstream>

struct pnm_image {
	pnm_image(std::string const& filename);

	pnm_image(pnm_image const&) = delete;

	pnm_image& operator=(pnm_image const&) = delete;

	~pnm_image() = default;

	void print_to_file(std::string const& filename);

private:
	std::vector<uint8_t> image_data;
	uint32_t type;
	uint32_t w, h;
	uint16_t depth;

	std::vector<uint8_t> buf;
	std::vector<uint8_t> uncompressed_data;

	void read_signature(std::ifstream& input);

	void read_chunk(std::ifstream& input, bool &is_end);

	void inflate();
};

#endif