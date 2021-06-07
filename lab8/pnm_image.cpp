#include <fstream>
#include <cstring>
#include <algorithm>
#include <cmath>
#include <exception>

#include "pnm_image.h"

double const PI = 3.1415926535;

struct pnm_image::huffman_tree {
	huffman_tree() {}

	~huffman_tree() {
		delete l;
		delete r;
	}

	bool add_element(uint8_t length, uint8_t symbol) {
		if (length == 0) {
			if (l == nullptr && r == nullptr && !is_leaf) {
				this->symbol = symbol;
				is_leaf = true;
				return true;
			} else {
				return false;
			}
		} else if (!is_leaf) {
			if (l == nullptr) l = new huffman_tree();
			if (l->add_element(length - 1, symbol)) return true;
			if (r == nullptr) r = new huffman_tree();
			if (r->add_element(length - 1, symbol)) return true;
			return false;
		} else {
			return false;
		}
	}

	huffman_tree* l = nullptr;
	huffman_tree* r = nullptr;
	uint8_t symbol = 0;
	bool is_leaf = false;
};

static size_t get_file_size(std::string const& filename) {
	std::ifstream input(filename, std::ios_base::binary | std::ios_base::ate);
	return input.tellg();
}

pnm_image::pnm_image(std::string const& filename) {
	std::ifstream input(filename, std::ios_base::binary);
	if (input.fail()) throw std::runtime_error("Could not open input file");
	uint32_t byte_size = get_file_size(filename);
	raw_data.resize(byte_size);
	input.read(reinterpret_cast<char*>(raw_data.data()), byte_size);
	if (input.fail()) throw std::runtime_error("Error while reading input JPEG file");
	while (true) {
		if(read_segment()) break;
	}
}

pnm_image::~pnm_image() {
	for (size_t i = 0; i < 4; i++) {
		if (ac_tree[i] != nullptr) delete ac_tree[i];
		if (dc_tree[i] != nullptr) delete dc_tree[i];
	}
}

uint32_t pnm_image::read_be() {
	if (pos + 2 > raw_data.size()) throw std::runtime_error("Unexpected end of file while reading marker");
	uint32_t ans = static_cast<uint32_t>(raw_data[pos]) * 256 + raw_data[pos + 1];
	pos += 2;
	return ans;
}

void pnm_image::decode_huffman_table() {
	pos += 2;
	uint8_t header = raw_data[pos++];
	size_t num = header % 16;
	if (num > 3) throw std::runtime_error("Incorrect number of HT table");
	bool is_ac = (header & 16);
	huffman_tree* root;
	if (!is_ac) {
		dc_tree[num] = new huffman_tree();
		root = dc_tree[num];
	} else {
		ac_tree[num] = new huffman_tree();
		root = ac_tree[num];
	}
	uint8_t lengths[16];
	for (size_t i = 0; i < 16; i++) lengths[i] = raw_data[pos++];
	std::vector<uint8_t> elements;
	for (size_t i = 0; i < 16; i++) {
		for (size_t j = 0; j < lengths[i]; j++) {
			elements.push_back(raw_data[pos++]);
		}
	}
	for (size_t i = 0, k = 0; i < 16; i++) {
		for (size_t j = 0; j < lengths[i]; j++) {
			if (!root->add_element(i + 1, elements[k++])) throw std::runtime_error("Could not add element to the Huffman tree");
		}
	}
}

void pnm_image::decode_quantization_table() {
	pos += 2;
	uint8_t header = raw_data[pos++];
	if (header / 16 != 0) throw std::runtime_error("Unsupported precision");
	std::vector<uint32_t> table(64);
	for (size_t i = 0; i < 64; i++) {
		table[i] = raw_data[pos++];
	}
	get_quant[header % 16] = table;
}

void pnm_image::decode_frame() {
	pos += 2;
	uint8_t precision = raw_data[pos++];
	if (precision != 8) throw std::runtime_error("Unsupported precision");
	h = read_be();
	w = read_be();
	depth = 255;
	image_data.resize(static_cast<size_t>(h) * w, 0);
	type = 1;
	uint8_t number_of_components = raw_data[pos++];
	if (number_of_components != 1) throw std::runtime_error("Only grey scaled images are supported");
	uint8_t component_id = raw_data[pos++];
	if (component_id != 1) throw std::runtime_error("Expected Y component");
	uint8_t sampling_factor = raw_data[pos++];
	uint8_t quantization_number = raw_data[pos++];
}

#include <iostream>

void pnm_image::decode_scan() {
	size_t len = read_be();
	pos += len - 2;
	std::vector<uint8_t> encoded_image;
	while (pos + 2 < raw_data.size()) {
		if (raw_data[pos] == 0xff) {
			if (raw_data[pos + 1] == 0) {
				encoded_image.push_back(raw_data[pos]);
			}
			pos += 2;
		} else {
			encoded_image.push_back(raw_data[pos++]);
		}
	}
	int32_t old_value = 0;
	size_t p = 0;
	for (size_t i = 0; i * 8 < h; i++) {
		for (size_t j = 0; j * 8 < w; j++) {
			get_matrix(i, j, old_value, encoded_image, p);
			if (cur_interval != -1) {
				cur_interval--;
				if (cur_interval == 0) {
					cur_interval = restart_interval;
					if (p % 8 != 0) p += 8 - (p % 8);
					old_value = 0;
				}
			}
		}
	}
}

static uint8_t get_bit(std::vector<uint8_t> const &bytes, size_t pos) {
	uint8_t byte = bytes[pos / 8];
	pos %= 8;
	return ((byte & (1 << (7 - pos))) == 0 ? 0 : 1);
}

static double alpha(double u) {
	if (u == 0) {
		return 1 / sqrt(2);
	} else {
		return 1;
	}
}

void pnm_image::get_matrix(size_t x, size_t y, int32_t &old_value, std::vector<uint8_t> &encoded_image, size_t &p) {
	int32_t matrix[8][8] =
	{
		 0,  1,  5,  6, 14, 15, 27, 28,
		 2,  4,  7, 13, 16, 26, 29, 42,
		 3,  8, 12, 17, 25, 30, 41, 43,
		 9, 11, 18, 24, 31, 40, 44, 53,
		10, 19, 23, 32, 39, 45, 52, 54,
		20, 22, 33, 38, 46, 51, 55, 60,
		21, 34, 37, 47, 50, 56, 59, 61,
		35, 36, 48, 49, 57, 58, 62, 63
	};
	std::vector<int32_t> decoded_values(64, 0);
	{
		huffman_tree* root = dc_tree[0];
		while (!root->is_leaf) {
			if (get_bit(encoded_image, p++) == 0) {
				root = root->l;
			} else {
				root = root->r;
			}
		}
		uint32_t bits = root->symbol;
		int32_t value = 0;
		for (size_t i = 0; i < bits; i++) {
			value = 2 * value + get_bit(encoded_image, p++);
		}
		bits = (1 << (bits - 1));
		if (value < bits) value -= 2 * bits - 1;
		old_value += value;
		decoded_values[0] = old_value * get_quant[0][0];
		size_t cur_num = 1;
		while (cur_num < 64) {
			root = ac_tree[0];
			while (!root->is_leaf) {
				if (get_bit(encoded_image, p++) == 0) {
					root = root->l;
				} else {
					root = root->r;
				}
			}
			bits = root->symbol;
			if (bits == 0) break;
			cur_num += bits / 16;
			bits %= 16;
			value = 0;
			for (size_t i = 0; i < bits; i++) {
				value = 2 * value + get_bit(encoded_image, p++);
			}
			if (cur_num < 64) {
				bits = (1 << (bits - 1));
				if (value < bits) value -= 2 * bits - 1;
				decoded_values[cur_num] = value * get_quant[0][cur_num];
				cur_num++;
			}
		}
	}
	for (size_t i = 0; i < 8; i++) {
		for (size_t j = 0; j < 8; j++) {
			matrix[i][j] = decoded_values[matrix[i][j]];
		}
	}
	double final_matrix[8][8];
	for (size_t x = 0; x < 8; x++) {
		for (size_t y = 0; y < 8; y++) {
			final_matrix[x][y] = 0;
			for (size_t u = 0; u < 7; u++) {
				for (size_t v = 0; v < 7; v++) {
					final_matrix[x][y] += alpha(u) * alpha(v) * matrix[u][v] * cos((2 * x + 1) * u * PI / 16) * cos((2 * y + 1) * v * PI / 16);
				}
			}
			final_matrix[x][y] /= 4;
		}
	}
	for (size_t i = 8 * x; i < std::min(static_cast<size_t>(h), 8 * (x + 1)); i++) {
		for (size_t j = 8 * y; j < std::min(static_cast<size_t>(w), 8 * (y + 1)); j++) {
			image_data[i * w + j] = ceil(final_matrix[i % 8][j % 8]) + 128;
		}
	}
}

bool pnm_image::read_segment() {
	uint32_t type = read_be();
	if (type >= 0xffe0 && type <= 0xffef) type = 0xffe0;
	uint32_t len;
	switch (type) {
	case 0xffd8:
		break;
	case 0xffc0:
		decode_frame();
		break;
	case 0xffc4:
		decode_huffman_table();
		break;
	case 0xffdb:
		decode_quantization_table();
		break;
	case 0xffda:
		decode_scan();
		break;
	case 0xffd9:
		return true;
	case 0xffdd:
		pos += 2;
		restart_interval = read_be();
		cur_interval = restart_interval;
		break;
	case 0xffe0:
		len = read_be();
		pos += len - 2;
		break;
	case 0xfffe:
		len = read_be();
		pos += len - 2;
		break;
	default:
		throw std::runtime_error("Unknown marker " + std::to_string(type));
	}
	return false;

}

void pnm_image::print_to_file(std::string const& filename) {
	std::ofstream output(filename, std::ios_base::binary);
	if (!output.is_open()) throw std::runtime_error("Could not open file for writing");
	output << (type == 1 ? "P5\n" : "P6\n");
	output << w << " " << h << "\n";
	output << depth << "\n";
	output.write(reinterpret_cast<char*>(image_data.data()), static_cast<size_t>(w) * h * type);
	if (output.fail()) {
		output.close();
		std::remove(filename.c_str());
		throw std::runtime_error("Could not write to the file");
	}
}