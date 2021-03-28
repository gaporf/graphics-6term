#include <string>
#include <fstream>
#include <exception>
#include <cstring>
#include <random>
#include <chrono>

#include "pgm_image.h"

static uint32_t get_number(const char* s) {
	char const* cur = s;
	if (*cur == '\0') throw std::runtime_error("Incorrect format of file");
	while (*cur != '\0') {
		if (!std::isdigit(*cur)) throw std::runtime_error("Incorrect format of file");
		cur++;
	}
	uint32_t num = std::stoul(s);
	if (num == 0) throw std::runtime_error("Incorrect format of file");
	return num;
}

pgm_image::pgm_image(std::string const& filename, char file_type) {
	std::ifstream input(filename, std::ios_base::binary);
	if (input.fail()) throw std::runtime_error("Could not open input file");
	char input_str[128];
	input.get(input_str, 128, '\n');
	if (strcmp(input_str, "P5") != 0) throw std::runtime_error("Incorret P5 file");
	input.ignore();
	input.get(input_str, 128, ' ');
	w = get_number(input_str);
	input.ignore();
	input.get(input_str, 128, '\n');
	h = get_number(input_str);
	input.ignore();
	input.get(input_str, 128, '\n');
	depth = get_number(input_str);
	size_t length = static_cast<size_t>(w) * h;
	try {
		data = std::unique_ptr<double[]>(new double[length]);
	} catch (...) {
		throw std::runtime_error("Could not allocate memory");
	}
	input.ignore();
	if (file_type == '0') {
		std::unique_ptr<uint8_t[]> buffer;
		try {
			buffer = std::unique_ptr<uint8_t[]>(new uint8_t[length]);
		} catch (...) {
			throw std::runtime_error("Could not allocate memory");
		}
		input.read(reinterpret_cast<char*>(buffer.get()), length);
		if (input.fail()) throw std::runtime_error("Incorrect format of file");
		input.ignore();
		if (!input.eof()) throw std::runtime_error("Incorrect foramt of file");
		for (size_t i = 0; i < length; i++) data[i] = static_cast<double>(buffer[i]);
	} else {
		double* ptr = data.get();
		for (size_t i = 0; i < h; i++) {
			double cur_color = 0;
			double diff = static_cast<double>(255) / (w - 1);
			for (size_t j = 0; j < w; j++, cur_color += diff) *ptr++ = cur_color;
		}
	}
}

static double get_round(uint8_t i_bit, uint8_t num_variants) {
	return static_cast<double>(i_bit) * 255 / num_variants;
}

static double get_real(double y, double gamma) {
	y /= 255;
	if (gamma == 0) {
		return (y <= 0.04045) ? y / 12.92 : pow((y + 0.055) / 1.055, 2.4);
	}
	return pow(y, gamma);
}

void pgm_image::no_dither(uint8_t num_bits, double gamma) {
	uint8_t const num_variants = (1ull << num_bits) - 1;
	double const div = static_cast<double>(255) / num_variants;
	for (size_t i = 0; i < static_cast<size_t>(w) * h; i++) {
		double pixel = data[i] / div;
		uint8_t left_bits = floor(pixel);
		if (left_bits == num_variants) {
			data[i] = 255;
			continue;
		}
		uint8_t right_bits = left_bits + 1;
		double left_round = get_round(left_bits, num_variants);
		double right_round = get_round(right_bits, num_variants);
		double left_real = get_real(left_round, gamma);	
		double right_real = get_real(right_round, gamma);
		double cur_real = get_real(data[i], gamma);
		data[i] = (std::abs(cur_real - left_real) <= std::abs(cur_real - right_real)) ? round(left_round) : round(right_round);
	}
}

void pgm_image::ordered_dither(uint8_t num_bits, double gamma) {
	uint8_t const num_variants = (1ull << num_bits) - 1;
	double const div = static_cast<double>(255) / num_variants;
	double matrix[8][8] = { 0, 32, 8, 40, 2, 34, 10, 42,
	                        48, 16, 56, 24, 50, 18, 58, 26,
		                    12, 44, 4, 36, 14, 46, 6, 38,
                            60, 28, 52, 20, 62, 30, 54, 22,
                            3, 35, 11, 43, 1, 33, 9, 41,
	                        51, 19, 59, 27, 49, 17, 57, 25,
	                        15, 47, 7, 39, 13, 45, 5, 37,
	                        63, 31, 55, 23, 61, 29, 53, 21 };
	for (size_t i = 0; i < 8; i++) {
		for (size_t j = 0; j < 8; j++) {
			matrix[i][j] = (matrix[i][j] + 1) / 65;
		}
	}
	for (size_t i = 0; i < h; i++) {
		for (size_t j = 0; j < w; j++) {
			size_t k = i * w + j;
			double pixel = data[k] / div;
			uint8_t left_bits = floor(pixel);
			if (left_bits == num_variants) {
				data[k] = 255;
				continue;
			}
			uint8_t right_bits = left_bits + 1;
			double left_round = get_round(left_bits, num_variants);
			double right_round = get_round(right_bits, num_variants);
			double left_real = get_real(left_round, gamma);
			double right_real = get_real(right_round, gamma);
			double cur_real = get_real(data[k], gamma);
			double diff = right_real - left_real;
			data[k] = (cur_real < left_real + diff * matrix[j % 8][i % 8]) ? round(left_round) : round(right_round);
		}
	}
}

void pgm_image::random_dither(uint8_t num_bits, double gamma) {
	std::mt19937_64 rnd;
	auto time_seed = std::chrono::high_resolution_clock::now().time_since_epoch().count();
	std::seed_seq seed{ static_cast<uint32_t>(time_seed & 0xFFFFFFFF), static_cast<uint32_t>(time_seed >> 32) };
	rnd.seed(seed);
	std::uniform_real_distribution<double> unif(0, 1);
	uint8_t const num_variants = (1ull << num_bits) - 1;
	double const div = static_cast<double>(255) / num_variants;
	for (size_t i = 0; i < static_cast<size_t>(w) * h; i++) {
		double pixel = data[i] / div;
		uint8_t left_bits = floor(pixel);
		if (left_bits == num_variants) {
			data[i] = 255;
			continue;
		}
		uint8_t right_bits = left_bits + 1;
		double left_round = get_round(left_bits, num_variants);
		double right_round = get_round(right_bits, num_variants);
		double left_real = get_real(left_round, gamma);
		double right_real = get_real(right_round, gamma);
		double cur_real = get_real(data[i], gamma);
		double diff = right_real - left_real;
		cur_real += diff * unif(rnd);
		data[i] = (cur_real >= right_real) ? round(right_round) : round(left_round);
	}
}

void pgm_image::floyd_steinberg_dither(uint8_t num_bits, double gamma) {
	std::unique_ptr<double[]> error;
	try {
		error = std::unique_ptr<double[]>(new double[static_cast<size_t>(w) * h]);
	} catch (...) {
		throw std::runtime_error("Could not allocate memory");
	}
	for (size_t i = 0; i < static_cast<size_t>(w) * h; i++) error[i] = 0;
	uint8_t const num_variants = (1ull << num_bits) - 1;
	double const div = static_cast<double>(255) / num_variants;
	for (size_t i = 0; i < h; i++) {
		for (size_t j = 0; j < w; j++) {
			size_t k = i * w + j;
			double pixel = data[k] / div;
			uint8_t left_bits = floor(pixel);
			if (left_bits == num_variants) {
				data[k] = 255;
				continue;
			}
			uint8_t right_bits = left_bits + 1;
			double left_round = get_round(left_bits, num_variants);
			double right_round = get_round(right_bits, num_variants);
			double left_real = get_real(left_round, gamma);
			double right_real = get_real(right_round, gamma);
			double cur_real = get_real(data[k], gamma);
			double diff = right_real - left_real;
			cur_real += diff * error[k];
			double quant_error;
			if (cur_real < right_real) {
				data[k] = round(left_round);
				quant_error = (cur_real - left_real) / diff;
			} else {
				data[k] = round(right_round);
				quant_error = (cur_real - right_real) / diff;
			}
			if (j + 1 < w) error[i * w + (j + 1)] += quant_error * 7 / 16;
			if (j > 0 && i + 1 < h) error[(i + 1) * w + (j - 1)] += quant_error * 3 / 16;
			if (i + 1 < h) error[(i + 1) * w + j] += quant_error * 5 / 16;
			if (i + 1 < h && j + 1 < w) error[(i + 1) * w + (j + 1)] += quant_error / 16;
		}
	}
}

void pgm_image::jarvis_judice_ninke_dither(uint8_t num_bits, double gamma) {
	std::unique_ptr<double[]> error;
	try {
		error = std::unique_ptr<double[]>(new double[static_cast<size_t>(w) * h]);
	} catch (...) {
		throw std::runtime_error("Could not allocate memory");
	}
	for (size_t i = 0; i < static_cast<size_t>(w) * h; i++) error[i] = 0;
	uint8_t const num_variants = (1ull << num_bits) - 1;
	double const div = static_cast<double>(255) / num_variants;
	for (size_t i = 0; i < h; i++) {
		for (size_t j = 0; j < w; j++) {
			size_t k = i * w + j;
			double pixel = data[k] / div;
			uint8_t left_bits = floor(pixel);
			if (left_bits == num_variants) {
				data[k] = 255;
				continue;
			}
			uint8_t right_bits = left_bits + 1;
			double left_round = get_round(left_bits, num_variants);
			double right_round = get_round(right_bits, num_variants);
			double left_real = get_real(left_round, gamma);
			double right_real = get_real(right_round, gamma);
			double cur_real = get_real(data[k], gamma);
			double diff = right_real - left_real;
			cur_real += diff * error[k];
			double quant_error;
			if (cur_real < right_real) {
				data[k] = round(left_round);
				quant_error = (cur_real - left_real) / diff;
			} else {
				data[k] = round(right_round);
				quant_error = (cur_real - right_real) / diff;
			}
			if (j + 1 < w) error[i * w + (j + 1)] += quant_error * 7 / 48;
			if (j + 2 < w) error[i * w + (j + 2)] += quant_error * 5 / 48;
			if (i + 1 < h) {
				if (j - 2 >= 0) error[(i + 1) * w + (j - 2)] += quant_error * 3 / 48;
				if (j - 1 >= 0) error[(i + 1) * w + (j - 1)] += quant_error * 5 / 48;
				error[(i + 1) * w + j] += quant_error * 7 / 48;
				if (j + 1 < w) error[(i + 1) * w + (j + 1)] += quant_error * 5 / 48;
				if (j + 2 < w) error[(i + 1) * w + (j + 2)] += quant_error * 3 / 48;
			}
			if (i + 2 < h) {
				if (j - 2 >= 0) error[(i + 2) * w + (j - 2)] += quant_error * 1 / 48;
				if (j - 1 >= 0) error[(i + 2) * w + (j - 1)] += quant_error * 3 / 48;
				error[(i + 2) * w + j] += quant_error * 5 / 48;
				if (j + 1 < w) error[(i + 2) * w + (j + 1)] += quant_error * 3 / 48;
				if (j + 2 < w) error[(i + 2) * w + (j + 2)] += quant_error * 1 / 48;
			}
		}
	}
}

void pgm_image::sierra_dither(uint8_t num_bits, double gamma) {
	std::unique_ptr<double[]> error;
	try {
		error = std::unique_ptr<double[]>(new double[static_cast<size_t>(w) * h]);
	} catch (...) {
		throw std::runtime_error("Could not allocate memory");
	}
	for (size_t i = 0; i < static_cast<size_t>(w) * h; i++) error[i] = 0;
	uint8_t const num_variants = (1ull << num_bits) - 1;
	double const div = static_cast<double>(255) / num_variants;
	for (size_t i = 0; i < h; i++) {
		for (size_t j = 0; j < w; j++) {
			size_t k = i * w + j;
			double pixel = data[k] / div;
			uint8_t left_bits = floor(pixel);
			if (left_bits == num_variants) {
				data[k] = 255;
				continue;
			}
			uint8_t right_bits = left_bits + 1;
			double left_round = get_round(left_bits, num_variants);
			double right_round = get_round(right_bits, num_variants);
			double left_real = get_real(left_round, gamma);
			double right_real = get_real(right_round, gamma);
			double cur_real = get_real(data[k], gamma);
			double diff = right_real - left_real;
			cur_real += diff * error[k];
			double quant_error;
			if (cur_real < right_real) {
				data[k] = round(left_round);
				quant_error = (cur_real - left_real) / diff;
			} else {
				data[k] = round(right_round);
				quant_error = (cur_real - right_real) / diff;
			}
			if (j + 1 < w) error[i * w + (j + 1)] += quant_error * 5 / 32;
			if (j + 2 < w) error[i * w + (j + 2)] += quant_error * 3 / 32;
			if (i + 1 < h) {
				if (j - 2 >= 0) error[(i + 1) * w + (j - 2)] += quant_error * 2 / 32;
				if (j - 1 >= 0) error[(i + 1) * w + (j - 1)] += quant_error * 4 / 32;
				error[(i + 1) * w + j] += quant_error * 5 / 32;
				if (j + 1 < w) error[(i + 1) * w + (j + 1)] += quant_error * 4 / 32;
				if (j + 2 < w) error[(i + 1) * w + (j + 2)] += quant_error * 2 / 32;
			}
			if (i + 2 < h) {
				if (j - 1 >= 0) error[(i + 2) * w + (j - 1)] += quant_error * 2 / 32;
				error[(i + 2) * w + j] += quant_error * 3 / 32;
				if (j + 1 < w) error[(i + 2) * w + (j + 1)] += quant_error * 2 / 32;
			}
		}
	}
}

void pgm_image::atkinson_dither(uint8_t num_bits, double gamma) {
	std::unique_ptr<double[]> error;
	try {
		error = std::unique_ptr<double[]>(new double[static_cast<size_t>(w) * h]);
	} catch (...) {
		throw std::runtime_error("Could not allocate memory");
	}
	for (size_t i = 0; i < static_cast<size_t>(w) * h; i++) error[i] = 0;
	uint8_t const num_variants = (1ull << num_bits) - 1;
	double const div = static_cast<double>(255) / num_variants;
	for (size_t i = 0; i < h; i++) {
		for (size_t j = 0; j < w; j++) {
			size_t k = i * w + j;
			double pixel = data[k] / div;
			uint8_t left_bits = floor(pixel);
			if (left_bits == num_variants) {
				data[k] = 255;
				continue;
			}
			uint8_t right_bits = left_bits + 1;
			double left_round = get_round(left_bits, num_variants);
			double right_round = get_round(right_bits, num_variants);
			double left_real = get_real(left_round, gamma);
			double right_real = get_real(right_round, gamma);
			double cur_real = get_real(data[k], gamma);
			double diff = right_real - left_real;
			cur_real += diff * error[k];
			double quant_error;
			if (cur_real < right_real) {
				data[k] = round(left_round);
				quant_error = (cur_real - left_real) / diff;
			} else {
				data[k] = round(right_round);
				quant_error = (cur_real - right_real) / diff;
			}
			if (j + 1 < w) error[i * w + (j + 1)] += quant_error / 8;
			if (j + 2 < w) error[i * w + (j + 2)] += quant_error / 8;
			if (i + 1 < h) {
				if (j - 1 >= 0) error[(i + 1) * w + (j - 1)] += quant_error / 8;
				error[(i + 1) * w + j] += quant_error / 8;
				if (j + 1 < w) error[(i + 1) * w + (j + 1)] += quant_error / 8;
			}
			if (i + 2 < h) {
				error[(i + 2) * w + j] += quant_error / 8;
			}
		}
	}
}

void pgm_image::halftone_dither(uint8_t num_bits, double gamma) {
	uint8_t const num_variants = (1ull << num_bits) - 1;
	double const div = static_cast<double>(255) / num_variants;
	double matrix[4][4] = { 7, 13, 11, 4,
							12, 16, 14, 8,
							10, 15, 6, 2,
							5, 9, 3, 1 };
	for (size_t i = 0; i < 4; i++) {
		for (size_t j = 0; j < 4; j++) {
			matrix[i][j] /= 17;
		}
	}
	for (size_t i = 0; i < h; i++) {
		for (size_t j = 0; j < w; j++) {
			size_t k = i * w + j;
			double pixel = data[k] / div;
			uint8_t left_bits = floor(pixel);
			if (left_bits == num_variants) {
				data[k] = 255;
				continue;
			}
			uint8_t right_bits = left_bits + 1;
			double left_round = get_round(left_bits, num_variants);
			double right_round = get_round(right_bits, num_variants);
			double left_real = get_real(left_round, gamma);
			double right_real = get_real(right_round, gamma);
			double cur_real = get_real(data[k], gamma);
			double diff = right_real - left_real;
			data[k] = (cur_real < left_real + diff * matrix[i % 4][j % 4]) ? round(left_round) : round(right_round);
		}
	}
}

void pgm_image::print_to_file(std::string const& filename, char dither_type, uint8_t num_bits, double gamma) {
	switch (dither_type) {
	case '0':
		no_dither(num_bits, gamma);
		break;
	case '1':
		ordered_dither(num_bits, gamma);
		break;
	case '2':
		random_dither(num_bits, gamma);
		break;
	case '3':
		floyd_steinberg_dither(num_bits, gamma);
		break;
	case '4':
		jarvis_judice_ninke_dither(num_bits, gamma);
		break;
	case '5':
		sierra_dither(num_bits, gamma);
		break;
	case '6':
		atkinson_dither(num_bits, gamma);
		break;
	case '7':
		halftone_dither(num_bits, gamma);
		break;
	default:
		throw std::runtime_error("Incorrect type of dithering");
	}
	std::ofstream output(filename, std::ios_base::binary);
	output << "P5\n";
	output << w << " " << h << "\n";
	output << depth << "\n";
	for (size_t i = 0; i < static_cast<size_t>(w) * h; i++) {
		uint8_t num = data[i];
		output.write(reinterpret_cast<char*>(&num), 1);
	}
	if (output.fail()) {
		output.close();
		std::remove(filename.c_str());
		throw std::runtime_error("Could not write to the file");
	}
}