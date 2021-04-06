#ifndef PNM_IMAGE_H
#define PNM_IMAGE_H

#include <memory>
#include <cstdint>
#include <string>

struct pnm_image {
	pnm_image(std::string const& filename);

	pnm_image(pnm_image const&) = delete;

	pnm_image& operator=(pnm_image const&) = delete;

	~pnm_image() = default;

	void convert(uint32_t new_w, uint32_t new_h, double dx, double dy, double gamma, char type, double B, double C);

	void print_to_file(std::string const& filename);

private:
	std::unique_ptr<double[]> data;
	uint32_t type;
	uint32_t w, h;
	uint16_t depth;

	void nearest(uint32_t new_w, uint32_t new_h, double dx, double dy);

	void bilinear(uint32_t new_w, uint32_t new_h, double dx, double dy, double gamma);

	void lanczos(uint32_t new_w, uint32_t new_h, double dx, double dy, double gamma);

	void bc_spline(uint32_t new_w, uint32_t new_h, double dx, double dy, double gamma, double B, double C);
};

#endif