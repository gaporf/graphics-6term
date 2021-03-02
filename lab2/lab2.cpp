#include <iostream>
#include <cstring>

#include "pnm_image.h"

int main(int argc, char* argv[]) {
	const std::string input_format = "Input format: -f <initial color model> -t <final color model> -i <number of input files> <name of input file> -o <number of output files> <name of output file>";
	if (argc != 11) {
		std::cerr << input_format << std::endl;
		return 1;
	}
	std::string initial_color_model;
	std::string final_color_model;
	std::string input_filename;
	size_t num_input_files = 0;
	std::string output_filename;
	size_t num_output_files = 0;
	for (size_t cur = 1; cur <= 10;) {
		if (strcmp(argv[cur], "-f") == 0 && cur + 1 < 11) {
			initial_color_model = argv[cur + 1];
			cur += 2;
		} else if (strcmp(argv[cur], "-t") == 0 && cur + 1 < 11) {
			final_color_model = argv[cur + 1];
			cur += 2;
		} else if (strcmp(argv[cur], "-i") == 0 && cur + 2 < 11) {
			if (argv[cur + 1][0] == '\0' || argv[cur + 1][1] != '\0') break;
			if (argv[cur + 1][0] == '1') {
				num_input_files = 1;
			} else if (argv[cur + 1][0] == '3') {
				num_input_files = 3;
			} else {
				break;
			}
			input_filename = argv[cur + 2];
			cur += 3;
		} else if (strcmp(argv[cur], "-o") == 0 && cur + 2 < 11) {
			if (argv[cur + 1][0] == '\0' || argv[cur + 1][1] != '\0') break;
			if (argv[cur + 1][0] == '1') {
				num_output_files = 1;
			} else if (argv[cur + 1][0] == '3') {
				num_output_files = 3;
			} else {
				break;
			}
			output_filename = argv[cur + 2];
			cur += 3;
		} else {
			break;
		}
	}
	if (initial_color_model == "" || final_color_model == "" || input_filename == "" || output_filename == "") {
		std::cerr << input_format << std::endl;
		return 1;
	}
	try {
		pnm_image image;
		if (num_input_files == 1) {
			image = std::move(pnm_image(input_filename, initial_color_model));
		} else {
			size_t pos = input_filename.find_first_of('.');
			if (pos == std::string::npos) throw std::runtime_error("Incorrect name of file");
			std::string pattern = input_filename.substr(0, pos);
			std::string extension = input_filename.substr(pos);
			image = std::move(pnm_image(
				pnm_image((pattern + "_1" + extension), initial_color_model),
				pnm_image((pattern + "_2" + extension), initial_color_model),
				pnm_image((pattern + "_3" + extension), initial_color_model)));
		}
		if (num_output_files == 1) {
			image.print_to_file(output_filename, final_color_model);
		} else {
			size_t pos = output_filename.find_first_of('.');
			if (pos == std::string::npos) throw std::runtime_error("Incorrect name of file");
			std::string pattern = output_filename.substr(0, pos);
			std::string extension = output_filename.substr(pos);
			image.print_to_files(pattern, extension, final_color_model);
		}
	} catch (std::exception& e) {
		std::cerr << e.what() << std::endl;
		return 1;
	}
	return 0;
}