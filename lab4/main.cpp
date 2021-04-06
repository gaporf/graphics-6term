#include <iostream>
#include <string>
#include <exception>

#include "pnm_image.h"

template<typename T>
T get_valid_number(char const* arg, T(*to_number)(char const*, size_t*), std::string const& var_name, std::string const& type_name) {
	try {
		size_t index;
		T ans = to_number(arg, &index);
		if (arg[index] != '\0') throw std::runtime_error("");
		return ans;
	} catch (...) {
		throw std::runtime_error("Invalid " + var_name + ", expected valid " + type_name + " number");
	}
}

uint32_t get_uint32_t(char const* arg, size_t* index) {
	if (arg[0] == '-') throw std::runtime_error("Requested width of height should be positive integer");
	return std::stoul(arg, index);
}

double get_double(char const* arg, size_t* index) {
	return std::stod(arg, index);
}

int main(int argc, char* argv[]) {
	std::string const input_format = "Input format: <input file> <output file> <requested width> <requested height> <dx> <dy> <gamma> <scale> [<B> <C>]";
	if ((argc != 9) && (argc != 11)) {
		std::cerr << input_format << std::endl;
		return 1;
	}
	uint32_t new_w;
	uint32_t new_h;
	double dx;
	double dy;
	double gamma;
	try {
		new_w = get_valid_number<uint32_t>(argv[3], get_uint32_t, "requested w", "positive integer");
		if (new_w == 0) throw std::runtime_error("Requested w should be positive integer");
		new_h = get_valid_number<uint32_t>(argv[4], get_uint32_t, "requested h", "positive integer");
		if (new_h == 0) throw std::runtime_error("Requested h should be positive integer");
		dx = get_valid_number<double>(argv[5], get_double, "dx", "double");
		dy = get_valid_number<double>(argv[6], get_double, "dy", "double");
		gamma = get_valid_number<double>(argv[7], get_double, "gamma", "double");
		if (gamma < 0) throw std::runtime_error("gamma should be non-negative integer");
	} catch (std::exception& e) {
		std::cerr << e.what() << std::endl;
		return 1;
	}
	if (argv[8][1] != '\0') {
		std::cerr << "scale should be from 0 to 3" << std::endl;
		return 1;
	}
	double B = 0;
	double C = 0.5;
	if ((argc == 11) && (argv[8][0] != '3')) {
		std::cerr << "B and C are valid only for BC-spline" << std::endl;
		return 1;
	}
	if (argc == 11) {
		try {
			B = get_valid_number<double>(argv[9], get_double, "B", "double");
			C = get_valid_number<double>(argv[10], get_double, "C", "double");
		} catch (std::exception &e) {
			std::cerr << e.what() << std::endl;
			return 1;
		}
	}
	try {
		pnm_image image(argv[1]);
		image.convert(new_w, new_h, dx, dy, gamma, argv[8][0], B, C);
		image.print_to_file(argv[2]);
	} catch (std::exception& e) {
		std::cerr << e.what() << std::endl;
		return 1;
	}
	return 0;
}