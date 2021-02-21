#include <iostream>
#include <fstream>
#include <string>
#include <exception>
#include <algorithm>

static uint32_t get_number(char const* s) {
	char const* cur = s;
	if (*cur == '\0') {
		throw std::exception("Incorrect format of file");
	}
	while (*cur != '\0') {
		if (!std::isdigit(*cur)) {
			throw std::exception("Incorrect format of file");
		}
		cur++;
	}
	uint32_t num = std::stoul(s);
	if (num == 0) {
		throw std::exception("Incorrect format of file");
	}
	return num;
}

struct ppm_image {
	explicit ppm_image(char const* filename) {
		std::ifstream input(filename, std::ios_base::binary);
		if (input.fail()) {
			throw std::exception("Could not open input file");
		}
		char input_str[128];
		input.get(input_str, 128, '\n');
		if (strcmp(input_str, "P5") == 0) {
			type = 1;
		} else if (strcmp(input_str, "P6") == 0) {
			type = 3;
		} else {
			throw std::exception("Incorrect format of file");
		}
		input.ignore();
		input.get(input_str, 128, ' ');
		w = get_number(input_str);
		input.ignore();
		input.get(input_str, 128, '\n');
		h = get_number(input_str);
		input.ignore();
		input.get(input_str, 128, '\n');
		depth = get_number(input_str);
		size_t length = static_cast<size_t>(w) * h * type;
		try {
			data = new uint8_t[length];
		} catch (...) {
			throw std::exception("Could not allocate memory");
		}
		input.ignore();
		input.read(reinterpret_cast<char*>(data), length);
		if (input.fail()) {
			throw std::exception("Incorrect format of file");
		}
		input.ignore();
		if (!input.eof()) {
			throw std::exception("Incorrect format of file");
		}
	}

	~ppm_image() {
		delete data;
	}

	void print_to_file(char const* filename) {
		std::ofstream output(filename, std::ios_base::binary);
		if (!output.is_open()) {
			throw std::exception("Could not open the file");
		}
		output << 'P' << (type == 1 ? '5' : '6') << '\n';
		output << w << ' ' << h << '\n';
		output << depth << '\n';
		output.write(reinterpret_cast<char*> (data), static_cast<size_t>(w) * h * type);
		if (output.fail()) {
			output.close();
			std::remove(filename);
			throw std::exception("Could not write to the file");
		}
	}

	void invert() {
		size_t length = static_cast<size_t>(w) * h * type;
		for (size_t i = 0; i < length; i++) {
			data[i] ^= 255;
		}
	}

	void flip_horizontally() {
		for (size_t y = 0; y < h; y++) {
			size_t x1 = 0;
			size_t x2 = w - 1;
			while (x1 < x2) {
				for (size_t k = 0; k < type; k++) {
					std::swap(data[y * w * type + x1 * type + k], data[y * w * type + x2 * type + k]);
				}
				x1++;
				x2--;
			}
		}
	}

	void flip_vertically() {
		for (size_t x = 0; x < w; x++) {
			size_t y1 = 0;
			size_t y2 = h - 1;
			while (y1 < y2) {
				for (size_t k = 0; k < type; k++) {
					std::swap(data[y1 * w * type + x * type + k], data[y2 * w * type + x * type + k]);
				}
				y1++;
				y2--;
			}
		}
	}

	void rotate_90_clockwise() {
		size_t length = static_cast<size_t>(w) * h * type;
		try {
			uint8_t* new_data = new uint8_t[length];
			for (size_t x = 0; x < w; x++) {
				for (size_t y = 0; y < h; y++) {
					for (size_t k = 0; k < type; k++) {
						new_data[x * h * type + (h - y - 1) * type + k] = data[y * w * type + x * type + k];
					}
				}
			}
			std::swap(w, h);
			delete data;
			data = new_data;
		} catch (...) {
			throw std::exception("Could not allocate memory");
		}
	}

	void rotate_90_counter_clockwise() {
		size_t length = static_cast<size_t>(w) * h * type;
		try {
			uint8_t* new_data = new uint8_t[length];
			for (size_t x = 0; x < w; x++) {
				for (size_t y = 0; y < h; y++) {
					for (size_t k = 0; k < type; k++) {
						new_data[x * h * type + y * type + k] = data[y * w * type + (w - x - 1) * type + k];
					}
				}
			}
			std::swap(w, h);
			delete data;
			data = new_data;
		} catch (...) {
			throw std::exception("Could not allocate memory");
		}
	}

private:
	uint8_t* data = nullptr;
	uint32_t type;
	uint32_t w, h;
	uint16_t depth;
};

int main(int argc, char* argv[]) {
	if (argc != 4) {
		std::cerr << "Input format: <input file> <output file> <type of operation>" << std::endl;
		return 1;
	}
	try {
		ppm_image image(argv[1]);
		if (argv[3][0] == '\0' || argv[3][1] != '\0') {
			std::cerr << "Put the number from 0 to 4 as a third argument" << std::endl;
			return 1;
		}
		switch (argv[3][0]) {
		case '0':
			image.invert();
			break;
		case '1':
			image.flip_horizontally();
			break;
		case '2':
			image.flip_vertically();
			break;
		case '3':
			image.rotate_90_clockwise();
			break;
		case '4':
			image.rotate_90_counter_clockwise();
			break;
		default:
			std::cerr << "Put the number from 0 to 4 as a third argument" << std::endl;
			return 1;
		}
		image.print_to_file(argv[2]);
	} catch (std::exception& e) {
		std::cerr << e.what() << std::endl;
		return 1;
	}
	return 0;
}