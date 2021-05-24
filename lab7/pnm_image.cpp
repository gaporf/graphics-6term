#include <fstream>
#include <cstring>
#include <algorithm>
#include <cmath>
#include <exception>

#include "zlib/zlib.h"
#include "pnm_image.h"

static uint32_t read_be(void* buf) {
	uint8_t* num = reinterpret_cast<uint8_t*>(buf);
	return 16'777'216u * num[0] + 65'536u * num[1] + 256u * num[2] + num[3];
}

pnm_image::pnm_image(std::string const& filename) : type(-1), w(-1), h(-1), depth(-1) {
	std::ifstream input(filename, std::ios_base::binary);
	if (input.fail()) throw std::runtime_error("Could not open input file");
	read_signature(input);
	while (true) {
		bool is_end;
		read_chunk(input, is_end);
		if (is_end) break;
	}
	inflate();
	size_t scanline_len = static_cast<size_t>(type) * w + 1;
	if (uncompressed_data.size() != h * scanline_len) throw std::runtime_error("Incorrect length of uncompressed data");
	for (size_t i = 0; i < h; i++) {
		switch (uncompressed_data[scanline_len * i]) {
		case 0:
			for (size_t j = 0; j < w; j++) {
				for (size_t k = 0; k < type; k++) {
					image_data[i * type * w + j * type + k] = uncompressed_data[i * scanline_len + 1 + j * type + k];
				}
			}
			break;
		case 1:
			for (size_t j = 0; j < w; j++) {
				for (size_t k = 0; k < type; k++) {
					uint8_t left = (j == 0 ? 0 : image_data[i * type * w + (j - 1) * type + k]);
					image_data[i * type * w + j * type + k] = left + uncompressed_data[i * scanline_len + 1 + j * type + k];
				}
			}
			break;
		case 2:
			for (size_t j = 0; j < w; j++) {
				for (size_t k = 0; k < type; k++) {
					uint8_t up = (i == 0 ? 0 : image_data[(i - 1) * type * w + j * type + k]);
					image_data[i * type * w + j * type + k] = up + uncompressed_data[i * scanline_len + 1 + j * type + k];
				}
			}
			break;
		case 3:
			for (size_t j = 0; j < w; j++) {
				for (size_t k = 0; k < type; k++) {
					uint8_t left = (j == 0 ? 0 : image_data[i * type * w + (j - 1) * type + k]);
					uint8_t up = (i == 0 ? 0 : image_data[(i - 1) * type * w + j * type + k]);
					image_data[i * type * w + j * type + k] = (static_cast<uint16_t>(left) + up) / 2 + uncompressed_data[i * scanline_len + 1 + j * type + k];
				}
			}
			break;
		case 4:
			for (size_t j = 0; j < w; j++) {
				for (size_t k = 0; k < type; k++) {
					uint8_t left = (j == 0 ? 0 : image_data[i * type * w + (j - 1) * type + k]);
					uint8_t up = (i == 0 ? 0 : image_data[(i - 1) * type * w + j * type + k]);
					uint8_t left_up = (i == 0 || j == 0 ? 0 : image_data[(i - 1) * type * w + (j - 1) * type + k]);
					int16_t p = static_cast<int16_t>(left) + up - left_up;
					int16_t pa = std::abs(p - left);
					int16_t pb = std::abs(p - up);
					int16_t pc = std::abs(p - left_up);
					uint8_t final_pixel;
					if (pa <= pb && pa <= pc) {
						final_pixel = left + uncompressed_data[i * scanline_len + 1 + j * type + k];
					} else if (pb <= pc) {
						final_pixel = up + uncompressed_data[i * scanline_len + 1 + j * type + k];
					} else {
						final_pixel = left_up + uncompressed_data[i * scanline_len + 1 + j * type + k];
					}
					image_data[i * type * w + j * type + k] = final_pixel;
				}
			}
			break;
		default:
			throw std::runtime_error("Incorrect filter type");
		}
	}
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

void pnm_image::read_signature(std::ifstream& input) {
	char input_str[128];
	input.read(input_str, 8);
	if (input.fail()) throw std::runtime_error("Invalid signature of PNG file");
	if (static_cast<uint8_t>(input_str[0]) != static_cast<uint8_t>(0x89)) {
		throw std::runtime_error("PNG file must start with 0x89");
	}
	if (input_str[1] != 'P' || input_str[2] != 'N' || input_str[3] != 'G') {
		throw std::runtime_error("There is no \"PNG\" in the signarute");
	}
	if (static_cast<uint8_t>(input_str[4]) != static_cast<uint8_t>(0x0d)
		|| static_cast<uint8_t>(input_str[5]) != static_cast<uint8_t>(0x0a)) {
		throw std::runtime_error("There is no DOS-style line ending");
	}
	if (static_cast<uint8_t>(input_str[6]) != static_cast<uint8_t>(0x1a)) {
		throw std::runtime_error("There is no stop displaying under DOS");
	}
	if (static_cast<uint8_t>(input_str[7]) != static_cast<uint8_t>(0x0a)) {
		throw std::runtime_error("There is no line ending in the signature");
	}
}

static uint32_t get_crc(uint8_t* type, uint8_t* buf, size_t length) {
	uint32_t table[256];
	uint32_t polynomial = 0xedb88320;
	for (size_t i = 0; i < 256; i++) {
		uint32_t c = i;
		for (size_t j = 0; j < 8; j++) {
			if (c % 2 == 0) {
				c >>= 1;
			} else {
				c = polynomial ^ (c >> 1);
			}
		}
		table[i] = c;
	}
	uint32_t crc = 0xffffffff;
	for (size_t i = 0; i < 4; i++) {
		crc = table[(crc ^ type[i]) & 0xff] ^ (crc >> 8);
	}
	for (size_t i = 0; i < length; i++) {
		crc = table[(crc ^ buf[i]) & 0xff] ^ (crc >> 8);
	}
	return crc ^ 0xffffffff;
}

void pnm_image::read_chunk(std::ifstream& input, bool &is_end) {
	uint8_t str_length[4];
	input.read(reinterpret_cast<char*>(str_length), 4);
	if (input.fail()) throw std::runtime_error("Could not read chunk length");
	uint32_t length = read_be(str_length);
	char type_chunk[5];
	input.read(type_chunk, 4);
	type_chunk[4] = '\0';
	if (input.fail()) throw std::runtime_error("Could not read chunk type");
	std::vector<uint8_t> data(length);
	input.read(reinterpret_cast<char*>(data.data()), length);
	if (input.fail()) throw std::runtime_error("Could not read data chunk");
	uint8_t str_crc[4];
	input.read(reinterpret_cast<char*>(str_crc), 4);
	if (input.fail()) throw std::runtime_error("Could not read chunk crc");
	uint32_t crc = read_be(str_crc);
	if (crc != get_crc(reinterpret_cast<uint8_t*>(type_chunk), data.data(), length)) throw std::runtime_error("CRC is not correct");
	is_end = false;
	if (strcmp(type_chunk, "IHDR") == 0) {
		if (w != -1) throw std::runtime_error("Another header in the png file");
		if (length != 13) throw std::runtime_error("Incorrect length of header");
		w = read_be(data.data());
		h = read_be(data.data() + 4);
		if (data.data()[8] != 8) throw std::runtime_error("Expected 8-bit depth");
		if (data.data()[9] == 0) {
			type = 1;
		} else if (data.data()[9] == 2) {
			type = 3;
		} else {
			throw std::runtime_error("Expected PNG type 0 or 2");
		}
		depth = 255;
		if (data.data()[10] != 0) throw std::runtime_error("Expected 0 compression method");
		if (data.data()[11] != 0) throw std::runtime_error("Expected standard filter method");
		if (data.data()[12] != 0) throw std::runtime_error("Expected standard interlaced method");
		image_data.resize(w * h * type, 0);
		return;
	}
	if (w == -1) throw std::runtime_error("Expected header chunk");
	if (strcmp(type_chunk, "IDAT") == 0) {
		for (size_t i = 0; i < length; i++) buf.push_back(data.data()[i]);
	} else if (strcmp(type_chunk, "PLTE") == 0) {
		if (type == 1) throw std::runtime_error("PLTE can't be within PNG type 0");
	} else if (strcmp(type_chunk, "IEND") == 0) {
		if (length != 0) throw std::runtime_error("The last chunk should be empty");
		input.ignore();
		if (!input.eof()) throw std::runtime_error("There should not be any data after last chunk");
		is_end = true;
		return;
	}
}

void pnm_image::inflate() {
	z_stream stream;
	stream.zalloc = Z_NULL;
	stream.zfree = Z_NULL;
	stream.opaque = Z_NULL;
	stream.avail_in = 0;
	stream.next_in = Z_NULL;
	int ret = inflateInit(&stream);
	if (ret != Z_OK) throw std::runtime_error("Could not initiate z_stream");
	size_t const K = 2048;
	do {
		stream.avail_in = buf.size();
		stream.next_in = buf.data();
		uint8_t out[K];
		do {
			stream.avail_out = K;
			stream.next_out = out;
			ret = ::inflate(&stream, Z_NO_FLUSH);
			if (ret == Z_STREAM_ERROR) throw std::runtime_error("Could not inflate");
			if (ret == Z_NEED_DICT || ret == Z_DATA_ERROR || ret == Z_MEM_ERROR) throw std::runtime_error("Error while inflating");
			size_t have = K - stream.avail_out;
			for (size_t i = 0; i < have; i++) {
				uncompressed_data.push_back(out[i]);
			}
		} while (stream.avail_out == 0);
	} while (ret != Z_STREAM_END);
	inflateEnd(&stream);
}