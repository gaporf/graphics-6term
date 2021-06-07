#ifndef PNM_IMAGE_H
#define PNM_IMAGE_H

#include <vector>
#include <cstdint>
#include <string>
#include <fstream>
#include <map>

struct pnm_image {
	pnm_image(std::string const& filename);

	pnm_image(pnm_image const&) = delete;

	pnm_image& operator=(pnm_image const&) = delete;

	~pnm_image();

	void print_to_file(std::string const& filename);

private:
	std::vector<uint8_t> image_data;
	uint32_t type;
	uint32_t w, h;
	uint16_t depth;
	
	size_t restart_interval = -1;
	size_t cur_interval = -1;

	struct huffman_tree;

	std::vector<uint8_t> raw_data;
	size_t pos = 0;
	std::map<size_t, std::vector<uint32_t>> get_quant;
	std::map<size_t, huffman_tree*> ac_tree;
	std::map<size_t, huffman_tree*> dc_tree;

	bool read_segment();

	uint32_t read_be();

	void decode_huffman_table();

	void decode_quantization_table();

	void decode_frame();

	void decode_scan();

	void get_matrix(size_t x, size_t y, int32_t &old_value, std::vector<uint8_t> &encoded_image, size_t &p);
};

#endif