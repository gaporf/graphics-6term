#include <fstream>
#include <algorithm>
#include <cmath>
#include <cstring>

#include "pnm_image.h"

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

pnm_image::pnm_image(std::string const& filename) {
	std::ifstream input(filename, std::ios_base::binary);
	if (input.fail()) throw std::runtime_error("Could not open input file");
	char input_str[128];
	input.get(input_str, 128, '\n');
	if (strcmp(input_str, "P5") == 0) {
		type = 1;
	} else if (strcmp(input_str, "P6") == 0) {
		type = 3;
	} else {
		throw std::runtime_error("Expected P5 or P6");
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
	input.ignore();
	size_t length = static_cast<size_t>(w) * h * type;
	try {
		data = std::unique_ptr<double[]>(new double[length]);
	} catch (...) {
		throw std::runtime_error("Could not allocate memory");
	}
	std::unique_ptr<uint8_t[]> buffer;
	try {
		buffer = std::unique_ptr<uint8_t[]>(new uint8_t[length]);
	} catch (...) {
		throw std::runtime_error("Could not allocate memory");
	}
	input.read(reinterpret_cast<char*>(buffer.get()), length);
	if (input.fail()) throw std::runtime_error("Incorrect format of file");
	input.ignore();
	if (!input.eof()) throw std::runtime_error("Incorrect format of file");
	for (size_t i = 0; i < length; i++) data[i] = static_cast<double>(buffer[i]);
}

static double get_real(double y, double gamma) {
	y /= 255;
	if (gamma == 0) {
		return (y <= 0.04045) ? y / 12.92 : pow((y + 0.055) / 1.055, 2.4);
	}
	return pow(y, gamma);
}

static double from_real(double y, double gamma) {
	if (gamma == 0) {
		return (y <= 0.0031308 ? 12.92 * y : 1.055 * pow(y, 1 / 2.4) - 0.055);
	}
	return pow(y, 1 / gamma);
}

void pnm_image::convert(uint32_t new_w, uint32_t new_h, double dx, double dy, double gamma, char type, double B, double C) {
	switch (type) {
	case '0':
		nearest(new_w, new_h, dx, dy);
		break;
	case '1':
		bilinear(new_w, new_h, dx, dy, gamma);
		break;
	case '2':
		lanczos(new_w, new_h, dx, dy, gamma);
		break;
	case '3':
		bc_spline(new_w, new_h, dx, dy, gamma, B, C);
		break;
	default:
		throw std::runtime_error("Scale should be from 0 to 3");
	}
}

void pnm_image::nearest(uint32_t new_w, uint32_t new_h, double dx, double dy) {
	std::unique_ptr<double[]> new_data;
	try {
		new_data = std::unique_ptr<double[]>(new double[static_cast<size_t>(new_w) * new_h * type]);
	} catch (...) {
		throw std::runtime_error("Could not allocate memory");
	}
	double lx = std::max(-0.5, -0.5 + 2 * dx);
	double rx = std::min(w - 0.5, w - 0.5 + 2 * dx);
	double dlx = rx - lx;
	double ly = std::max(-0.5, -0.5 + 2 * dy);
	double ry = std::min(h - 0.5, h - 0.5 + 2 * dy);
	double dly = ry - ly;
	for (size_t i = 0; i < new_h; i++) {
		for (size_t j = 0; j < new_w; j++) {
			double real_x = round(lx + (j + 0.5) * dlx / static_cast<double>(new_w));
			double real_y = round(ly + (i + 0.5) * dly / static_cast<double>(new_h));
			size_t x = static_cast<size_t>(real_x);
			size_t y = static_cast<size_t>(real_y);
			for (size_t k = 0; k < type; k++) {
				new_data[(i * new_w + j) * type + k] = data[(y * w + x) * type + k];
			}
		}
	}
	data = std::move(new_data);
	w = new_w;
	h = new_h;
}

void pnm_image::bilinear(uint32_t new_w, uint32_t new_h, double dx, double dy, double gamma) {
	std::unique_ptr<double[]> new_data;
	try {
		new_data = std::unique_ptr<double[]>(new double[static_cast<size_t>(new_w) * h * type]);
	} catch (...) {
		throw std::runtime_error("Could not allocate memory");
	}
	double lx = std::max(-0.5, -0.5 + 2 * dx);
	double rx = std::min(w - 0.5, w - 0.5 + 2 * dx);
	double dlx = rx - lx;
	if (new_w >= w) {
		for (size_t i = 0; i < h; i++) {
			for (size_t j = 0; j < new_w; j++) {
				double real_x = lx + (j + 0.5) * dlx / new_w;
				size_t y = i;
				for (size_t k = 0; k < type; k++) {
					double color;
					if (real_x < 0) {
						size_t x = 0;
						color = data[(y * w + x) * type + k];
					} else if (real_x < w - 1) {
						size_t left_x = static_cast<size_t>(real_x);
						size_t right_x = left_x + 1;
						color = (real_x - left_x) * get_real(data[(y * w + right_x) * type + k], gamma) + (right_x - real_x) * get_real(data[(y * w + left_x) * type + k], gamma);
						color = from_real(color, gamma) * 255;
					} else {
						size_t x = w - 1;
						color = data[(y * w + x) * type + k];
					}
					new_data[(i * new_w + j) * type + k] = color;
				}
			}
		}
	} else {
		for (size_t i = 0; i < h; i++) {
			for (size_t j = 0; j < new_w; j++) {
				double real_x = lx + (j + 0.5) * dlx / new_w;
				double delta = static_cast<double>(w) / new_w;
				size_t y = i;
				for (size_t k = 0; k < type; k++) {
					double color = 0;
					double norm = 0;
					double left_x = real_x - delta;
					double right_x = real_x + delta;
					for (size_t new_x = ceil(std::max(0.0, left_x)); new_x <= std::min(static_cast<double>(w - 1), right_x); new_x++) {
						double mult = (new_x < real_x ? (new_x - left_x) / delta : (right_x - new_x) / delta);
						norm += mult;
						color += mult * get_real(data[(y * w + new_x) * type + k], gamma);
					}
					color = from_real(color / norm, gamma) * 255;
					new_data[(i * new_w + j) * type + k] = color;
				}
			}
		}
	}
	data = std::move(new_data);
	w = new_w;
	try {
		new_data = std::unique_ptr<double[]>(new double[static_cast<size_t>(w) * new_h * type]);
	} catch (...) {
		throw std::runtime_error("Could not allocate memory");
	}
	double ly = std::max(-0.5, -0.5 + 2 * dy);
	double ry = std::min(h - 0.5, h - 0.5 + 2 * dy);
	double dly = ry - ly;
	if (new_h >= h) {
		for (size_t i = 0; i < new_h; i++) {
			for (size_t j = 0; j < new_w; j++) {
				size_t x = j;
				double real_y = ly + (i + 0.5) * dly / new_h;
				for (size_t k = 0; k < type; k++) {
					double color;
					if (real_y < 0) {
						size_t y = 0;
						color = data[(y * w + x) * type + k];
					} else if (real_y < h - 1) {
						size_t left_y = static_cast<size_t>(real_y);
						size_t right_y = left_y + 1;
						color = (real_y - left_y) * get_real(data[(right_y * w + x) * type + k], gamma) + (right_y - real_y) * get_real(data[(left_y * w + x) * type + k], gamma);
						color = round(from_real(color, gamma) * 255);
					} else {
						size_t y = h - 1;
						color = data[(y * w + x) * type + k];
					}
					new_data[(i * new_w + j) * type + k] = color;
				}
			}
		}
	} else {
		for (size_t i = 0; i < new_h; i++) {
			for (size_t j = 0; j < new_w; j++) {
				size_t x = j;
				double real_y = ly + (i + 0.5) * dly / new_h;
				double delta = static_cast<double>(h) / new_h;
				for (size_t k = 0; k < type; k++) {
					double color = 0;
					double norm = 0;
					double left_y = real_y - delta;
					double right_y = real_y + delta;
					for (size_t new_y = ceil(std::max(0.0, left_y)); new_y <= std::min(static_cast<double>(h - 1), right_y); new_y++) {
						double mult = (new_y < real_y ? (new_y - left_y) / delta : (right_y - new_y) / delta);
						norm += mult;
						color += mult * get_real(data[(new_y * w + x) * type + k], gamma);
					}
					color = from_real(color / norm, gamma) * 255;
					new_data[(i * new_w + j) * type + k] = color;
				}
			}
		}
	}
	data = std::move(new_data);
	h = new_h;
}

static double get_L(double x) {
	double const PI = 3.14159265359;
	if (x == 0) return 1;
	if (x < -3 || x > 3) return 0;
	return 3 * sin(PI * x) * sin(PI * x / 3) / pow(PI * x, 2);
}

void pnm_image::lanczos(uint32_t new_w, uint32_t new_h, double dx, double dy, double gamma) {
	std::unique_ptr<double[]> new_data;
	try {
		new_data = std::unique_ptr<double[]>(new double[static_cast<size_t>(new_w) * h * type]);
	} catch (...) {
		throw std::runtime_error("Could not allocate memory");
	}
	double lx = std::max(-0.5, -0.5 + 2 * dx);
	double rx = std::min(w - 0.5, w - 0.5 + 2 * dx);
	double dlx = rx - lx;
	if (new_w >= w) {
		for (size_t i = 0; i < h; i++) {
			for (size_t j = 0; j < new_w; j++) {
				double real_x = lx + (j + 0.5) * dlx / new_w;
				size_t y = i;
				for (size_t k = 0; k < type; k++) {
					double color = 0;
					double down_x = floor(real_x);
					while (real_x - down_x <= 3) {
						color += get_real(data[(y * w + static_cast<size_t>(std::max(0.0, down_x))) * type + k], gamma) * get_L(down_x - real_x);
						down_x--;
					}
					double up_x = ceil(real_x);
					if (std::abs(up_x - real_x) < 1e-9) up_x++;
					while (up_x - real_x <= 3) {
						color += get_real(data[(y * w + static_cast<size_t>(std::min(static_cast<double>(w - 1), up_x))) * type + k], gamma) * get_L(up_x - real_x);
						up_x++;
					}
					if (color < 0) color = 0;
					if (color > 1) color = 1;
					color = from_real(color, gamma) * 255;
					new_data[(i * new_w + j) * type + k] = color;
				}
			}
		}
	} else {
		for (size_t i = 0; i < h; i++) {
			for (size_t j = 0; j < new_w; j++) {
				double real_x = lx + (j + 0.5) * dlx / new_w;
				size_t y = i;
				double delta = static_cast<double>(w) / new_w;
				for (size_t k = 0; k < type; k++) {
					double color = 0;
					double norm = 0;
					double left_x = real_x - 3 * delta;
					double right_x = real_x + 3 * delta;
					for (size_t new_x = ceil(std::max(0.0, left_x)); new_x <= std::min(static_cast<double>(w - 1), right_x); new_x++) {
						double mult = get_L((new_x - real_x) / delta);
						norm += mult;
						color += mult * get_real(data[(y * w + new_x) * type + k], gamma);
					}
					color /= norm;
					if (color < 0) color = 0;
					if (color > 1) color = 1;
					color = from_real(color, gamma) * 255;
					new_data[(i * new_w + j) * type + k] = color;
				}
			}
		}
	}
	data = std::move(new_data);
	w = new_w;
	try {
		new_data = std::unique_ptr<double[]>(new double[static_cast<size_t>(w) * new_h * type]);
	} catch (...) {
		throw std::runtime_error("Could not allocate memory");
	}
	double ly = std::max(-0.5, -0.5 + 2 * dy);
	double ry = std::min(h - 0.5, h - 0.5 + 2 * dy);
	double dly = ry - ly;
	if (new_h >= h) {
		for (size_t i = 0; i < new_h; i++) {
			for (size_t j = 0; j < new_w; j++) {
				size_t x = j;
				double real_y = ly + (i + 0.5) * dly / new_h;
				for (size_t k = 0; k < type; k++) {
					double color = 0;
					double down_y = floor(real_y);
					while (real_y - down_y <= 3) {
						color += get_real(data[(static_cast<size_t>(std::max(0.0, down_y)) * w + x) * type + k], gamma) * get_L(down_y - real_y);
						down_y--;
					}
					double up_y = ceil(real_y);
					if (std::abs(up_y - real_y) < 1e-9) up_y++;
					while (up_y - real_y <= 3) {
						color += get_real(data[(static_cast<size_t>(std::min(static_cast<double>(h - 1), up_y)) * w + x) * type + k], gamma) * get_L(up_y - real_y);
						up_y++;
					}
					if (color < 0) color = 0;
					if (color > 1) color = 1;
					color = from_real(color, gamma) * 255;
					new_data[(i * new_w + j) * type + k] = color;
				}
			}
		}
	} else {
		for (size_t i = 0; i < new_h; i++) {
			for (size_t j = 0; j < new_w; j++) {
				size_t x = j;
				double real_y = ly + (i + 0.5) * dly / new_h;
				double delta = static_cast<double>(h) / new_h;
				for (size_t k = 0; k < type; k++) {
					double color = 0;
					double norm = 0;
					double left_y = real_y - 3 * delta;
					double right_y = real_y + 3 * delta;
					for (size_t new_y = ceil(std::max(0.0, left_y)); new_y <= std::min(static_cast<double>(h - 1), right_y); new_y++) {
						double mult = get_L((new_y - real_y) / delta);
						norm += mult;
						color += mult * get_real(data[(new_y * w + x) * type + k], gamma);
					}
					color /= norm;
					if (color < 0) color = 0;
					if (color > 1) color = 1;
					color = from_real(color, gamma) * 255;
					new_data[(i * new_w + j) * type + k] = color;
				}
			}
		}
	}
	data = std::move(new_data);
	h = new_h;
}

static double get_K(double x, double B, double C) {
	double res;
	if (std::abs(x) < 1) {
		res = (12 - 9 * B - 6 * C) * pow(std::abs(x), 3) + (-18 + 12 * B + 6 * C) * pow(std::abs(x), 2) + (6 - 2 * B);
	} else if (std::abs(x) < 2) {
		res = (-B - 6 * C) * pow(std::abs(x), 3) + (6 * B + 30 * C) * pow(std::abs(x), 2) + (-12 * B - 48 * C) * std::abs(x) + (8 * B + 24 * C);
	} else {
		res = 0;
	}
	return res / 6;
}

void pnm_image::bc_spline(uint32_t new_w, uint32_t new_h, double dx, double dy, double gamma, double B, double C) {
	std::unique_ptr<double[]> new_data;
	try {
		new_data = std::unique_ptr<double[]>(new double[static_cast<size_t>(new_w) * h * type]);
	} catch (...) {
		throw std::runtime_error("Could not allocate memory");
	}
	double lx = std::max(-0.5, -0.5 + 2 * dx);
	double rx = std::min(w - 0.5, w - 0.5 + 2 * dx);
	double dlx = rx - lx;
	if (new_w >= w) {
		for (size_t i = 0; i < h; i++) {
			for (size_t j = 0; j < new_w; j++) {
				double real_x = lx + (j + 0.5) * dlx / new_w;
				size_t y = i;
				for (size_t k = 0; k < type; k++) {
					double color = 0;
					double down_x = floor(real_x);
					while (real_x - down_x <= 2) {
						color += get_real(data[(y * w + static_cast<size_t>(std::max(0.0, down_x))) * type + k], gamma) * get_K(down_x - real_x, B, C);
						down_x--;
					}
					double up_x = ceil(real_x);
					if (std::abs(up_x - real_x) < 1e-9) up_x++;
					while (up_x - real_x <= 2) {
						color += get_real(data[(y * w + static_cast<size_t>(std::min(static_cast<double>(w - 1), up_x))) * type + k], gamma) * get_K(up_x - real_x, B, C);
						up_x++;
					}
					if (color < 0) color = 0;
					if (color > 1) color = 1;
					color = from_real(color, gamma) * 255;
					new_data[(i * new_w + j) * type + k] = color;
				}
			}
		}
	} else {
		for (size_t i = 0; i < h; i++) {
			for (size_t j = 0; j < new_w; j++) {
				double real_x = lx + (j + 0.5) * dlx / new_w;
				size_t y = i;
				double delta = static_cast<double>(w) / new_w;
				for (size_t k = 0; k < type; k++) {
					double color = 0;
					double norm = 0;
					double left_x = real_x - 2 * delta;
					double right_x = real_x + 2 * delta;
					for (size_t new_x = ceil(std::max(0.0, left_x)); new_x <= std::min(static_cast<double>(w - 1), right_x); new_x++) {
						double mult = get_K((new_x - real_x) / delta, B, C);
						norm += mult;
						color += mult * get_real(data[(y * w + new_x) * type + k], gamma);
					}
					color /= norm;
					if (color < 0) color = 0;
					if (color > 1) color = 1;
					color = from_real(color, gamma) * 255;
					new_data[(i * new_w + j) * type + k] = color;
				}
			}
		}
	}
	data = std::move(new_data);
	w = new_w;
	try {
		new_data = std::unique_ptr<double[]>(new double[static_cast<size_t>(new_w) * new_h * type]);
	} catch (...) {
		throw std::runtime_error("Could not allocate memory");
	}
	double ly = std::max(-0.5, -0.5 + 2 * dy);
	double ry = std::min(h - 0.5, h - 0.5 + 2 * dy);
	double dly = ry - ly;
	if (new_h >= h) {
		for (size_t i = 0; i < new_h; i++) {
			for (size_t j = 0; j < new_w; j++) {
				size_t x = j;
				double real_y = ly + (i + 0.5) * dly / new_h;
				for (size_t k = 0; k < type; k++) {
					double color = 0;
					double down_y = floor(real_y);
					while (real_y - down_y <= 2) {
						color += get_real(data[(static_cast<size_t>(std::max(0.0, down_y)) * w + x) * type + k], gamma) * get_K(down_y - real_y, B, C);
						down_y--;
					}
					double up_y = ceil(real_y);
					if (std::abs(up_y - real_y) < 1e-9) up_y++;
					while (up_y - real_y <= 2) {
						color += get_real(data[(static_cast<size_t>(std::min(static_cast<double>(h - 1), up_y)) * w + x) * type + k], gamma) * get_K(up_y - real_y, B, C);
						up_y++;
					}
					if (color < 0) color = 0;
					if (color > 1) color = 1;
					color = from_real(color, gamma) * 255;
					new_data[(i * new_w + j) * type + k] = color;
				}
			}
		}
	} else {
		for (size_t i = 0; i < new_h; i++) {
			for (size_t j = 0; j < new_w; j++) {
				size_t x = j;
				double real_y = ly + (i + 0.5) * dly / new_h;
				double delta = static_cast<double>(h) / new_h;
				for (size_t k = 0; k < type; k++) {
					double color = 0;
					double norm = 0;
					double left_y = real_y - 2 * delta;
					double right_y = real_y + 2 * delta;
					for (size_t new_y = ceil(std::max(0.0, left_y)); new_y <= std::min(static_cast<double>(h - 1), right_y); new_y++) {
						double mult = get_K((new_y - real_y) / delta, B, C);
						norm += mult;
						color += mult * get_real(data[(new_y * w + x) * type + k], gamma);
					}
					color /= norm;
					if (color < 0) color = 0;
					if (color > 1) color = 1;
					color = from_real(color, gamma) * 255;
					new_data[(i * new_w + j) * type + k] = color;
				}
			}
		}
	}
	data = std::move(new_data);
	h = new_h;
}

void pnm_image::print_to_file(std::string const& filename) {
	std::ofstream output(filename, std::ios_base::binary);
	if (!output.is_open()) throw std::runtime_error("Could not open file for writing");
	output << (type == 1 ? "P5\n" : "P6\n");
	output << w << " " << h << "\n";
	output << depth << "\n";
	for (size_t i = 0; i < static_cast<size_t>(w) * h * type; i++) {
		uint8_t num = static_cast<uint8_t>(data[i]);
		output.write(reinterpret_cast<char*>(&num), 1);
	}
	if (output.fail()) {
		output.close();
		std::remove(filename.c_str());
		throw std::runtime_error("Could not write to the file");
	}
}