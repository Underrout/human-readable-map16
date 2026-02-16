#include "c_api.h"
#include "human_readable_map16.h"

extern "C" void to_map16(const char *input_folder, const char *output_file) {
    HumanReadableMap16::to_map16::convert(input_folder, output_file);
}

extern "C" void from_map16(const char *input_file, const char *output_folder) {
    HumanReadableMap16::from_map16::convert(input_file, output_folder);
}
