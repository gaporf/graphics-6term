#include <string>
#include <fstream>
#include <exception>
#include <limits>
#include <cstring>
#include <vector>
#include <cctype>

#include "pgm_image.h"

static uint32_t get_number(char const* s) {
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

pgm_image::pgm_image(std::string const& filename) {
	std::ifstream input(filename, std::ios_base::binary);
	if (input.fail()) throw std::runtime_error("Could not open input file");
	char input_str[128];
	input.get(input_str, 128, '\n');
	if (strcmp(input_str, "P5") != 0) throw std::runtime_error("Incorrect P5 file");
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
		data = std::unique_ptr<uint8_t[]>(new uint8_t[length]);
	} catch (...) {
		throw std::runtime_error("Could not allocate memory");
	}
	input.ignore();
	input.read(reinterpret_cast<char*>(data.get()), length);
	if (input.fail()) throw std::runtime_error("Incorrect format of file");
	input.ignore();
	if (!input.eof()) throw std::runtime_error("Incorrect format of file");
}

static bool get_next(bool(&arr)[256]) {
	if (!arr[255]) {
		for (size_t i = 255; i > 0; i--) {
			if (arr[i - 1]) {
				arr[i - 1] = false;
				arr[i] = true;
				return true;
			}
		}
	} else {
		size_t j = 255;
		while (j > 0 && arr[j]) j--;
		for (size_t i = j; i > 0; i--) {
			if (arr[i - 1]) {
				arr[i - 1] = false;
				arr[i] = true;
				memset(arr + i + 1, 1, 255 - j);
				memset(arr + i + 256 - j, 0, j - i);
				return true;
			}
		}
	}
	return false;
}

void pgm_image::divide_into_classes(uint32_t classes) {
	classes = std::min(256u, classes - 1);
	double ans = std::numeric_limits<double>::min();
	bool ans_arr[256];
	memset(ans_arr, 0, 256);
	bool arr[256];
	double p[256];
	for (size_t i = 0; i < 256; i++) {
		p[i] = 0;
	}
	size_t length = static_cast<size_t>(w) * h;
	for (size_t i = 0; i < length; i++) {
		p[data[i]] += 1.0 / length;
	}
	std::memset(arr, 1, classes);
	std::memset(arr + classes, 0, 256 - classes);
	do {
		std::vector<double> q(classes + 1, 0);
		for (size_t i = 0, j = 0; i < 256; i++) {
			q[j] += p[i];
			if (arr[i]) j++;
		}
		std::vector<double> mu(classes + 1, 0);
		double average_mu = 0;
		for (size_t i = 0, j = 0; i < 256; i++) {
			average_mu += i * p[i];
			if (q[j] != 0) mu[j] += i * p[i] / q[j];
			if (arr[i]) j++;
		}
		double cur_ans = 0;
		for (size_t i = 0; i <= classes; i++) {
			cur_ans += q[i] * pow(mu[i] - average_mu, 2);
		}
		if (cur_ans > ans) {
			ans = cur_ans;
			for (size_t i = 0; i < 256; i++) ans_arr[i] = arr[i];
		}
	} while (get_next(arr));
	uint8_t cur_color = 0;
	uint8_t right_color[256];
	for (size_t i = 0, j = 0; i < 256; i++) {
		right_color[i] = cur_color;
		if (ans_arr[i]) {
			j++;
			cur_color = 255 * j / classes;
		}
	}
	for (size_t i = 0; i < length; i++) {
		data[i] = right_color[data[i]];
	}
}

void pgm_image::print_to_file(std::string const& filename) {
	std::ofstream output(filename, std::ios_base::binary);
	if (!output.is_open()) throw std::runtime_error("Could not open file for writing");
	output << "P5\n";
	output << w << " " << h << "\n";
	output << depth << "\n";
	output.write(reinterpret_cast<char*>(data.get()), static_cast<size_t>(w) * h);
	if (output.fail()) {
		output.close();
		std::remove(filename.c_str());
		throw std::runtime_error("Could not write to the file");
	}
}