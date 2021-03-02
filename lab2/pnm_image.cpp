#include <fstream>
#include <cstring>
#include <string>
#include <cctype>
#include <exception>
#include <cmath>

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

static void check_model(const std::string& color_model) {
	if (color_model != "RGB" && color_model != "HSL" && color_model != "HSV" 
		&& color_model != "YCbCr.601" && color_model != "YCbCr.709" 
		&& color_model != "YCoCg" && color_model != "CMY") {
		throw std::runtime_error("Unsupported color model");
	}
}

pnm_image::pnm_image(const std::string &filename, const std::string &color_model) : color_model(color_model) {
	check_model(color_model);
	std::ifstream input(filename, std::ios_base::binary);
	if (input.fail()) throw std::runtime_error("Could not open input file");
	char input_str[128];
	input.get(input_str, 128, '\n');
	if (strcmp(input_str, "P5") == 0) {
		type = 1;
	} else if (strcmp(input_str, "P6") == 0) {
		type = 3;
	} else {
		throw std::runtime_error("Incorrect format of file, expected P5 or P6");
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

pnm_image::pnm_image(const pnm_image& first, const pnm_image& second, const pnm_image& third) : 
	color_model(first.color_model), type(3), w(first.w), h(first.h), depth(first.depth) {
	if (first.type != 1 || second.type != 1 || third.type != 1
		|| first.w != second.w || second.w != third.w
		|| first.h != second.h || second.h != third.h
		|| first.depth != second.depth || second.depth != third.depth
		|| first.color_model != second.color_model || second.color_model != third.color_model) {
		throw std::runtime_error("Incorrect format of 3 files");
	}
	size_t length = static_cast<size_t>(w) * h;
	try {
		data = std::unique_ptr<uint8_t[]>(new uint8_t[length * 3]);
	} catch (...) {
		throw std::runtime_error("Could not allocate memory");
	}
	uint8_t* f = first.data.get();
	uint8_t* s = second.data.get();
	uint8_t* t = third.data.get();
	uint8_t* cur = data.get();
	for (size_t i = 0; i < length; i++) {
		*(cur++) = *(f++);
		*(cur++) = *(s++);
		*(cur++) = *(t++);
	}
}

void pnm_image::print_to_file(const std::string &filename, const std::string &color_model) {
	convert(color_model);
	std::ofstream output(filename, std::ios_base::binary);
	if (!output.is_open()) throw std::runtime_error("Could not open file for writing");
	output << "P6\n";
	output << w << " " << h << "\n";
	output << depth << "\n";
	output.write(reinterpret_cast<char*>(data.get()), static_cast<size_t>(w) * h * 3);
	if (output.fail()) {
		output.close();
		std::remove(filename.c_str());
		throw std::runtime_error("Could not write to the file");
	}
}

void pnm_image::print_to_files(const std::string &pattern, const std::string &extension, const std::string &color_model) {
	convert(color_model);
	for (size_t k = 1; k <= 3; k++) {
		std::string filename = pattern + "_" + std::to_string(k) + extension;
		std::ofstream output(filename, std::ios_base::binary);
		if (!output.is_open()) throw std::runtime_error("Could not open file for writing");
		output << "P5\n";
		output << w << " " << h << "\n";
		output << depth << "\n";
		char* ptr = reinterpret_cast<char*>(data.get()) + k - 1;
		for (size_t i = 0; i < size_t(w) * h; i++) {
			output.write(ptr, 1);
			ptr += 3;
		}
		if (output.fail()) {
			output.close();
			std::remove(filename.c_str());
			throw std::runtime_error("Could not write to the file");
		}
	}
}

void pnm_image::convert(const std::string &color_model) {
	check_model(color_model);
	if (type != 3) throw std::runtime_error("Excepcted P6 file, found P5");
	if (this->color_model == color_model) return;
	convert_to_RGB();
	convert_from_RGB(color_model);
}

static double mod(double a, double num) {
	while (a < 0) a += num;
	while (a >= num) a -= num;
	return a;
}

void pnm_image::convert_to_RGB() {
	if (color_model == "RGB") return;
	uint8_t* ptr = data.get();
	size_t length = static_cast<size_t>(w) * h;
	for (size_t i = 0; i < length; i++) {
		if (color_model == "HSL") {
			double H = static_cast<double>(*ptr) / 255 * 360;
			if (H == 360) H = 0;
			double S = static_cast<double>(*(ptr + 1)) / 255;
			double L = static_cast<double>(*(ptr + 2)) / 255;
			double C = (1 - std::abs(2 * L - 1)) * S;
			double X = C * (1.0 - std::abs(mod(H / 60, 2) - 1.0));
			double m = L - C / 2.0;
			double r;
			double g;
			double b;
			if (H < 60) {
				r = C;
				g = X;
				b = 0;
			} else if (H < 120) {
				r = X;
				g = C;
				b = 0;
			} else if (H < 180) {
				r = 0;
				g = C;
				b = X;
			} else if (H < 240) {
				r = 0;
				g = X;
				b = C;
			} else if (H < 300) {
				r = X;
				g = 0;
				b = C;
			} else {
				r = C;
				g = 0;
				b = X;
			}
			r = (r + m) * 255;
			g = (g + m) * 255;
			b = (b + m) * 255;
			if (r < 0) r = 0;
			if (r > 255) r = 255;
			if (g < 0) g = 0;
			if (g > 255) g = 255;
			if (b < 0) b = 0;
			if (b > 255) b = 255;
			uint8_t R = static_cast<uint8_t>(round(r));
			uint8_t G = static_cast<uint8_t>(round(g));
			uint8_t B = static_cast<uint8_t>(round(b));
			*(ptr++) = R;
			*(ptr++) = G;
			*(ptr++) = B;
		} else if (color_model == "HSV") {
			double H = static_cast<double>(*ptr) / 255 * 360;
			if (H == 360) H = 0;
			double S = static_cast<double>(*(ptr + 1)) / 255;
			double V = static_cast<double>(*(ptr + 2)) / 255;
			double C = V * S;
			double X = C * (1 - std::abs(mod(H / 60, 2) - 1));
			double m = V - C;
			double r;
			double g;
			double b;
			if (H < 60) {
				r = C;
				g = X;
				b = 0;
			} else if (H < 120) {
				r = X;
				g = C;
				b = 0;
			} else if (H < 180) {
				r = 0;
				g = C;
				b = X;
			} else if (H < 240) {
				r = 0;
				g = X;
				b = C;
			} else if (H < 300) {
				r = X;
				g = 0;
				b = C;
			} else {
				r = C;
				g = 0;
				b = X;
			}
			r = (r + m) * 255;
			g = (g + m) * 255;
			b = (b + m) * 255;
			if (r < 0) r = 0;
			if (r > 255) r = 255;
			if (g < 0) g = 0;
			if (g > 255) g = 255;
			if (b < 0) b = 0;
			if (b > 255) b = 255;
			uint8_t R = static_cast<uint8_t>(round(r));
			uint8_t G = static_cast<uint8_t>(round(g));
			uint8_t B = static_cast<uint8_t>(round(b));
			*(ptr++) = R;
			*(ptr++) = G;
			*(ptr++) = B;
		} else if (color_model == "YCbCr.601") {
			double Y = static_cast<double>(*ptr);
			double Cb = static_cast<double>(*(ptr + 1));
			double Cr = static_cast<double>(*(ptr + 2));
			double r = Y + (Cr - 128) * 1.402;
			double g = Y - (Cb - 128) * 0.344136 - (Cr - 128) * 0.714136;
			double b = Y + (Cb - 128) * 1.772;
			if (r < 0) r = 0;
			if (r > 255) r = 255;
			if (g < 0) g = 0;
			if (g > 255) g = 255;
			if (b < 0) b = 0;
			if (b > 255) b = 255;
			uint8_t R = static_cast<uint8_t>(round(r));
			uint8_t G = static_cast<uint8_t>(round(g));
			uint8_t B = static_cast<uint8_t>(round(b));
			*(ptr++) = R;
			*(ptr++) = G;
			*(ptr++) = B;
		} else if (color_model == "YCbCr.709") {
			double Y = static_cast<double>(*ptr);
			double Cb = static_cast<double>(*(ptr + 1));
			double Cr = static_cast<double>(*(ptr + 2));
			double y = (Y / 255 * 219) + 16;
			double cb = (Cb / 255 * 224) + 16;
			double cr = (Cr / 255 * 224) + 16;
			double r = y + (cr - 128) * 1.539648;
			double g = y - (cb - 128) * 0.1831429 - (cr - 128) * 0.457675;
			double b = y + (cb - 128) * 1.81418;
			if (r < 16) r = 16;
			if (r > 235) r = 235;
			if (g < 16) g = 16;
			if (g > 235) g = 235;
			if (b < 16) b = 16;
			if (b > 235) b = 235;
			r = (r - 16) / 219 * 255;
			g = (g - 16) / 219 * 255;
			b = (b - 16) / 219 * 255;
			uint8_t R = static_cast<uint8_t>(round(r));
			uint8_t G = static_cast<uint8_t>(round(g));
			uint8_t B = static_cast<uint8_t>(round(b));
			*(ptr++) = R;
			*(ptr++) = G;
			*(ptr++) = B;
		} else if (color_model == "YCoCg") {
			double Y = static_cast<double>(*ptr);
			double Co = static_cast<double>(*(ptr + 1));
			double Cg = static_cast<double>(*(ptr + 2));
			double y = Y / 255;
			double co = Co / 255 - 0.5;
			double cg = Cg / 255 - 0.5;
			double r = y + co - cg;
			double g = y + cg;
			double b = y - co - cg;
			r *= 255;
			g *= 255;
			b *= 255;
			if (r < 0) r = 0;
			if (r > 255) r = 255;
			if (g < 0) g = 0;
			if (g > 255) g = 255;
			if (b < 0) b = 0;
			if (b > 255) b = 255;
			uint8_t R = static_cast<uint8_t>(round(r));
			uint8_t G = static_cast<uint8_t>(round(g));
			uint8_t B = static_cast<uint8_t>(round(b));
			*(ptr++) = R;
			*(ptr++) = G;
			*(ptr++) = B;
		} else if (color_model == "CMY") {
			uint8_t c = *ptr;
			uint8_t m = *(ptr + 1);
			uint8_t y = *(ptr + 2);
			uint8_t r = 255 - c;
			uint8_t g = 255 - m;
			uint8_t b = 255 - y;
			*(ptr++) = r;
			*(ptr++) = g;
			*(ptr++) = b;
		}
	}
}

void pnm_image::convert_from_RGB(const std::string &color_model) {
	if (color_model == "RGB") return;
	uint8_t* ptr = data.get();
	size_t length = static_cast<size_t>(w) * h;
	for (size_t i = 0; i < length; i++) {
		if (color_model == "HSL") {
			uint8_t R = *ptr;
			uint8_t G = *(ptr + 1);
			uint8_t B = *(ptr + 2);
			double r = static_cast<double>(R) / 255;
			double g = static_cast<double>(G) / 255;
			double b = static_cast<double>(B) / 255;
			double cmax = std::max(std::max(r, g), b);
			double cmin = std::min(std::min(r, g), b);
			double delta = cmax - cmin;
			double H;
			double S;
			double L = (cmax + cmin) / 2;
			if (delta == 0) {
				H = 0;
			} else if (r == cmax) {
				H = 60 * mod((g - b) / delta, 6);
			} else if (g == cmax) {
				H = 60 * ((b - r) / delta + 2);
			} else {
				H = 60 * ((r - g) / delta + 4);
			}
			if (delta == 0) {
				S = 0;
			} else {
				S = delta / (1 - std::abs(2 * L - 1));
			}
			H = mod(H, 360);
			H = H / 360 * 255;
			S *= 255;
			L *= 255;
			if (S < 0) S = 0;
			if (S > 255) S = 255;
			if (L < 0) L = 0;
			if (L > 255) L = 255;
			uint8_t h = static_cast<uint8_t>(round(H));
			uint8_t s = static_cast<uint8_t>(round(S));
			uint8_t l = static_cast<uint8_t>(round(L));
			*(ptr++) = h;
			*(ptr++) = s;
			*(ptr++) = l;
		} else if (color_model == "HSV") {
			uint8_t R = *ptr;
			uint8_t G = *(ptr + 1);
			uint8_t B = *(ptr + 2);
			double r = static_cast<double>(R) / 255;
			double g = static_cast<double>(G) / 255;
			double b = static_cast<double>(B) / 255;
			double cmax = std::max(std::max(r, g), b);
			double cmin = std::min(std::min(r, g), b);
			double delta = cmax - cmin;
			double H;
			double S;
			double V = cmax;
			if (delta == 0) {
				H = 0;
			} else if (r == cmax) {
				H = 60 * mod((g - b) / delta, 6);
			} else if (g == cmax) {
				H = 60 * ((b - r) / delta + 2);
			} else {
				H = 60 * ((r - g) / delta + 4);
			}
			if (cmax == 0) {
				S = 0;
			} else {
				S = delta / cmax;
			}
			H = mod(H, 360);
			H = H / 360 * 255;
			S *= 255;
			V *= 255;
			if (S < 0) S = 0;
			if (S > 255) S = 255;
			if (V < 0) V = 0;
			if (V > 255) V = 255;
			uint8_t h = static_cast<uint8_t>(round(H));
			uint8_t s = static_cast<uint8_t>(round(S));
			uint8_t v = static_cast<uint8_t>(round(V));
			*(ptr++) = h;
			*(ptr++) = s;
			*(ptr++) = v;
		} else if (color_model == "YCbCr.601") {
			uint8_t R = *ptr;
			uint8_t G = *(ptr + 1);
			uint8_t B = *(ptr + 2);
			double r = static_cast<double>(R);
			double g = static_cast<double>(G);
			double b = static_cast<double>(B);
			double Y = 0.299 * r + 0.587 * g + 0.114 * b;
			double Cb = 128 - 0.168736 * r - 0.331264 * g + 0.5 * b;
			double Cr = 128 + 0.5 * r - 0.418688 * g - 0.081312 * b;
			if (Y < 0) Y = 0;
			if (Y > 255) Y = 255;
			if (Cb < 0) Cb = 0;
			if (Cb > 255) Cb = 255;
			if (Cr < 0) Cr = 0;
			if (Cr > 255) Cr = 255;
			uint8_t y = static_cast<uint8_t>(round(Y));
			uint8_t cb = static_cast<uint8_t>(round(Cb));
			uint8_t cr = static_cast<uint8_t>(round(Cr));
			*(ptr++) = y;
			*(ptr++) = cb;
			*(ptr++) = cr;
		} else if (color_model == "YCbCr.709") {
			uint8_t R = *ptr;
			uint8_t G = *(ptr + 1);
			uint8_t B = *(ptr + 2);
			double r = static_cast<double>(R);
			double g = static_cast<double>(G);
			double b = static_cast<double>(B);
			double Y = (46.742 * r + 157.243 * g + 15.874 * b) / 256 + 16;
			double Cb = (-25.765 * r - 86.674 * g + 112.439 * b) / 256 + 128;
			double Cr = (112.439 * r - 102.129 * g - 10.310 * b) / 256 + 128;
			Y = (Y - 16) / 219 * 255;
			Cb = (Cb - 16) / 224 * 255;
			Cr = (Cr - 16) / 224 * 255;
			if (Y < 0) Y = 0;
			if (Y > 255) Y = 255;
			if (Cb < 0) Cb = 0;
			if (Cb > 255) Cb = 255;
			if (Cr < 0) Cr = 0;
			if (Cr > 255) Cr = 255;
			uint8_t y = static_cast<uint8_t>(round(Y));
			uint8_t cb = static_cast<uint8_t>(round(Cb));
			uint8_t cr = static_cast<uint8_t>(round(Cr));
			*(ptr++) = y;
			*(ptr++) = cb;
			*(ptr++) = cr;
		} else if (color_model == "YCoCg") {
			uint8_t R = *ptr;
			uint8_t G = *(ptr + 1);
			uint8_t B = *(ptr + 2);
			double r = static_cast<double>(R) / 255;
			double g = static_cast<double>(G) / 255;
			double b = static_cast<double>(B) / 255;
			double y = 0.25 * r + 0.5 * g + 0.25 * b;
			double co = 0.5 * r - 0.5 * b;
			double cr = -0.25 * r + 0.5 * g - 0.25 * b;
			y *= 255;
			co = (co + 0.5) * 255;
			cr = (cr + 0.5) * 255;
			uint8_t Y = static_cast<uint8_t>(round(y));
			uint8_t Co = static_cast<uint8_t>(round(co));
			uint8_t Cr = static_cast<uint8_t>(round(cr));
			*(ptr++) = Y;
			*(ptr++) = Co;
			*(ptr++) = Cr;
		} else if (color_model == "CMY") {
			uint8_t r = *ptr;
			uint8_t g = *(ptr + 1);
			uint8_t b = *(ptr + 2);
			uint8_t c = 255 - r;
			uint8_t m = 255 - g;
			uint8_t y = 255 - b;
			*(ptr++) = c;
			*(ptr++) = m;
			*(ptr++) = y;
		}
	}
}