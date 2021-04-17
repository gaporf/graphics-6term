#include <iostream>
#include <exception>
#include <string>

#include "pgm_image.h"

int main(int argc, char* argv[]) {
	if (argc != 4) {
		std::cerr << "Input format: <input file> <output file> <num of classes>" << std::endl;
		return 1;
	}
	uint32_t classes;
	try {
		size_t index;
		classes = std::stoull(argv[3], &index);
		if (classes == 0 || argv[3][0] == '-' || argv[3][index] != '\0') throw std::runtime_error("");
	} catch (...) {
		std::cerr << "Number of classes should be a positive integer" << std::endl;
		return 1;
	}
	try {
		pgm_image image(argv[1]);
		image.divide_into_classes(classes);
		image.print_to_file(argv[2]);
	} catch (std::exception const& e) {
		std::cerr << e.what() << std::endl;
		return 1;
	}
	return 0;
}