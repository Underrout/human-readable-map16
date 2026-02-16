#include "c_api.h"
#include "human_map16_exception.h"
#include "human_readable_map16.h"
#include <exception>
#include <string>

thread_local std::string last_error;

extern "C" {
    
    int to_map16(const char *input_folder, const char *output_file) {
        try {
            HumanReadableMap16::to_map16::convert(input_folder, output_file);
            return 0;
        } catch (const HumanMap16Exception& e) {
            last_error = e.get_detailed_error_message();
            return -1;
        } catch (const std::exception& e) {
            last_error = e.what();
            return -1;
        } catch (...) {
            last_error = "unknown error";
            return -1;
        }
    }

    int from_map16(const char *input_file, const char *output_folder) {
        try {
            HumanReadableMap16::from_map16::convert(input_file, output_folder);
            return 0;
        } catch (const HumanMap16Exception& e) {
            last_error = e.get_detailed_error_message();
            return -1;
        } catch (const std::exception& e) {
            last_error = e.what();
            return -1;
        } catch (...) {
            last_error = "unknown error";
            return -1;
        }
    }

    const char* get_last_error() {
        return last_error.c_str();
    }
}

