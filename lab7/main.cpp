#include <iostream>
#include <exception>

#include "pnm_image.h"

int main(int argc, char* argv[]) {
	try {
		if (argc != 3) throw std::runtime_error("Input format: <input png file> <output pnm file>");
		pnm_image image(argv[1]);
		image.print_to_file(argv[2]);
	} catch (std::exception& e) {
		std::cerr << e.what() << std::endl;
		return 1;
	}
	return 0;
}