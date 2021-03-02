#ifndef PNM_IMAGE_H
#define PNM_IMAGE_H

#include <memory>
#include <cstdint>

struct pnm_image {
	pnm_image() : type(0), w(0), h(0), depth(0) {}

	pnm_image(const std::string &filename, const std::string &color_model);

	pnm_image(const pnm_image &first, const pnm_image &second, const pnm_image &third);

	pnm_image(const pnm_image&) = delete;

	pnm_image& operator=(const pnm_image&) = delete;

	pnm_image& operator=(pnm_image&&) = default;

	~pnm_image() = default;
	 
	void print_to_file(const std::string &filename, const std::string &color_model);

	void print_to_files(const std::string &pattern, const std::string &extension, const std::string &color_model);

private:
	std::unique_ptr<uint8_t[]> data;
	std::string color_model;
	uint32_t type;
	uint32_t w, h;
	uint16_t depth;

	void convert(const std::string &color_model);

	void convert_to_RGB();

	void convert_from_RGB(const std::string &color_model);
};

#endif
