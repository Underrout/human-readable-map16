#ifdef __cplusplus
extern "C" {
#endif

int from_map16(const char* input_file, const char* output_folder);
int to_map16(const char* input_folder, const char* output_file);

const char* get_last_error();

#ifdef __cplusplus
}
#endif
