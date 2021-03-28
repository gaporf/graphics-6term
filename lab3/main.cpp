#include <iostream>
#include <exception>
#include <string>

#include "pgm_image.h"

int main(int argc, char* argv[]) {
	if (argc != 7) {
		std::cerr << "input format: <input file> <output file> <gradient> <dithering> <bit> <gamma>" << std::endl;
		return 1;
	}
	if ((argv[3][0] != '0' && argv[3][0] != '1') || argv[3][1] != '\0') {
		std::cerr << "gradient should be 0 or 1" << std::endl;
		return 1;
	}
	try {
		pgm_image image(std::string(argv[1]), argv[3][0]);
		if (argv[4][1] != '\0' || argv[4][0] < '0' || argv[4][0] > '8') throw std::runtime_error("dithering should be from 0 to 8");
		if (argv[5][1] != '\0' || argv[5][0] < '1' || argv[5][0] > '8') throw std::runtime_error("num of bits should be from 1 to 8");
		size_t idx;
		double gamma;
		try {
			gamma = std::stod(argv[6], &idx);
		} catch (...) {
			throw std::runtime_error("gamma should be valid double number");
		}
		if (argv[6][idx] != '\0') throw std::runtime_error("gamma should be valid double number");
		image.print_to_file(argv[2], argv[4][0], argv[5][0] - '0', gamma);
	} catch (std::exception const& e) {
		std::cerr << e.what() << std::endl;
		return 1;
	}
	return 0;
}