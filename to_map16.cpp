#include "pch.h"
#include "human_readable_map16.h"
#include "filesystem_error.h"
#include "tile_error.h"
#include "header_error.h"
#include "arrays.h"

/*
* === PLAN ===
* 
* - Make a std::vector<Byte> for every block pointed to by the offset+size table
* 
* 1.  Check if input_path is a file or directory (can also just assume it's a directory and full game export for now)
* 
* 2a. If directory, it's a full game export, grab 
*     the header from the header.txt file in the directory
* 
* 2b. If it's a file, the header is at the start of the file, grab the header from there
*     (disregarding this case for now, focusing on full game exports only)
* 
* 3.  Iterate through pages in FG_pages and BG_pages folders, take tiles found and insert 
*     them into first block (pointer 0), take acts like settings found and move them into the 
*     second block (pointer 1), push zeros for anything not found (may fill in later, maybe)
* 
* 4.  Grab tileset_specific_tiles (page 2s), insert their tiles into the fifth block (pointer 4)
* 
* 5.  Grab tileset_group_specific_tiles (pages 0 & 1), insert their tiles into the sixth block (pointer 5),
*     in the first tileset group (tileset group 0), grab the diagonal pipe tiles and insert them into the eight 
*     block (pointer 7)
* 
* 6.  Grab normal pipe tiles from pipe_tiles and insert them into the seventh block (pointer 6)
*  
* 
* === ERROR HANDLING CONCEPT ===
* 
* - If an expected file/directory is not found, raise an exception
* 
* - Parse lines with sscanf, we know what format to expect at every tile offset in every file, so 
*   if a sscanf call fails, we do not need to try with different formats, we know something's wrong
* 
* - If sscanf fails, raise an exception that includes the offending line and any additional needed context 
*   (full, acts like only, tiles only, expected tile number, etc.) and let an error handler deal with it
* 
* - Error handler should be able to infer the exact problem, since it receives the whole offending line and 
*   context it was found in, main functionality does not need to deal with all this itself
* 
* - Never mind, functions should validate the line before sscanf'ing it, sscanf does not really validate 
*   formats and I don't want to use regex (they slow af) so prolly just do it with some stream or whatever
*/

/*
void HumanReadableMap16::to_map16::get_line_or_throw(std::fstream* stream, std::string& string, const fs::path file_path, unsigned int curr_line_number) {
	if (!std::getline(*stream, string)) {
		throw  DataError("Unexpected end of file", file_path, curr_line_number + 1, "", 0);
	}
}
*/

std::shared_ptr<HumanReadableMap16::Header> HumanReadableMap16::to_map16::parse_header_file(const fs::path header_path) {
	verify_header_file(header_path);

	FILE* fp;
	fopen_s(&fp, header_path.string().c_str(), "r");

	auto header = std::make_shared<Header>();

	unsigned int is_full_game_export, has_tileset_specific_page_2;

	fscanf_s(fp,
		"file_format_version_number: %X\n" \
		"game_id: %X\n" \
		"program_version: %X\n" \
		"program_id: %X\n" \
		"size_x: %X\n" \
		"size_y: %X\n" \
		"base_x: %X\n" \
		"base_y: %X\n" \
		"is_full_game_export: %X\n" \
		"has_tileset_specific_page_2: %X\n",
		&(header->file_format_version_number),
		&(header->game_id),
		&(header->program_version),
		&(header->program_id),
		&(header->size_x),
		&(header->size_y),
		&(header->base_x),
		&(header->base_y),
		&(is_full_game_export),
		&(has_tileset_specific_page_2)
	);

	header->various_flags_and_info = has_tileset_specific_page_2 | (is_full_game_export << 1);

	fclose(fp);

	/* does not work right now because ansi characters (like fusoya's copyright symbol...) are apparently invalid chars in strings
	
	std::fstream file;
	file.open(header_path);

	std::string line;

	while (std::getline(file, line)) {
		size_t pos = line.find("comment_field: \"", 0);
		if (pos != std::string::npos) {
			size_t end_pos = line.find("\"", pos);
			header->comment = std::string(line.substr(pos, end_pos));
		}
	}

	file.close();

	*/

	header->comment = "";

	return header;
}

void HumanReadableMap16::to_map16::verify_header_file(const fs::path header_path) {
	if (!fs::exists("header.txt")) {
		throw FilesystemError("Expected file appears to be missing", fs::path("header.txt"));
	}

	std::array HEADER_VARS{
		"file_format_version_number",
		"game_id",
		"program_version",
		"program_id",
		"size_x",
		"size_y",
		"base_x",
		"base_y",
		"is_full_game_export",
		"has_tileset_specific_page_2",
		"comment_field"
	};

	std::array HEADER_VAR_MAX_DIGITS{
		4,
		4,
		4,
		4,
		8,
		8,
		8,
		8,
		1,
		1
	};

	std::unordered_set<char> hex_digits{
		'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'
	};

	std::fstream header_file;
	header_file.open(header_path);

	std::vector<std::string> lines;
	std::string curr_line;
	while (std::getline(header_file, curr_line)) {
		lines.push_back(curr_line);
	}
	// std::string message, fs::path file, unsigned int line_number, std::string line, unsigned int char_index, std::string variable
	if (lines.size() != HEADER_VARS.size()) {
		throw HeaderError("Header file contains more lines than expected", "header.txt", 0, lines.at(0), 0, "");
	}

	unsigned int i = 0;
	for (const auto& line : lines) {
		std::string var_name = HEADER_VARS.at(i);
		size_t curr_char = 0;

		std::string found_var_name;
		try {
			found_var_name = line.substr(0, var_name.size() + 2);
		} catch (const std::out_of_range&) {
			throw HeaderError("Unexpected end of header variable name", "header.txt", i, line, curr_char, var_name);
		}

		if (found_var_name != var_name + ": ") {
			throw HeaderError("Unexpected header variable/separator encountered for this line", "header.txt", i, line, curr_char, var_name);
		}

		curr_char += var_name.size() + 2;

		if (var_name != "comment_field") {
			size_t var_size = HEADER_VAR_MAX_DIGITS.at(i);

			std::string var_value;
			try {
				var_value = line.substr(var_name.size() + 2, line.size() - (var_name.size() + 2));
			} catch (const std::out_of_range&) {
				throw HeaderError("Unexpected end of header value specification", "header.txt", i, line, curr_char, var_name);
			}

			size_t seen_size = 0;
			for (const auto c : var_value) {
				if (hex_digits.count(c) == 0) {
					throw HeaderError("Non-hexadecimal character encountered in header variable value", "header.txt", i, line, curr_char, var_name);
				}
				if (++seen_size > var_size) {
					throw HeaderError("Header value longer than expected and/or unexpected trailing whitespace", "header.txt", i, line, curr_char, var_name);
				}
				++curr_char;
			}

			if (var_name == "is_full_game_export" || var_name == "has_tileset_specific_page_2") {
				if (var_value.at(0) != '1' && var_value.at(0) != '0') {
					throw HeaderError("This header value may only be 1 (set to true) or 0 (set to false)", "header.txt", i, line, curr_char - 1, var_name);
				}
			}
		}
		else {
			std::string comment_including_apostrophes;
			try {
				comment_including_apostrophes = line.substr(var_name.size() + 2, line.size() - (var_name.size() + 2));
			} catch (const std::out_of_range&) {
				throw HeaderError("Unexpected end of value while verifying comment field", "header.txt", i, line, curr_char, var_name);
			}
			if (comment_including_apostrophes.at(0) != '"') {
				throw HeaderError("Comment field missing opening \" character", "header.txt", i, line, curr_char, var_name);
			}
			curr_char += comment_including_apostrophes.size();
			if (comment_including_apostrophes.at(comment_including_apostrophes.size() - 1) != '"') {
				throw HeaderError("Comment field missing closing \" character", "header.txt", i, line, curr_char, var_name);
			}
		}
		++i;
	}
}

void HumanReadableMap16::to_map16::split_and_insert_2(_2Bytes bytes, std::vector<Byte>& byte_vec) {
	byte_vec.push_back(bytes & 0xFF);
	byte_vec.push_back((bytes & 0xFF00) >> 8);
}

void HumanReadableMap16::to_map16::split_and_insert_4(_4Bytes bytes, std::vector<Byte>& byte_vec) {
	split_and_insert_2(bytes & 0xFFFF, byte_vec);
	split_and_insert_2((bytes & 0xFFFF0000) >> 16, byte_vec);
}

bool HumanReadableMap16::to_map16::try_LM_empty_convert_full(std::vector<Byte>& tiles_vec, std::vector<Byte>& acts_like_vec, 
	const std::string line, unsigned int expected_tile_number) {
	char buf[256];
	sprintf_s(buf, LM_EMTPY_TILE_FORMAT_NO_NEWLINE, expected_tile_number);
	std::string expected_line = buf;

	if (expected_line != line) {
		return false;
	}

	for (unsigned int i = 0; i != 4; i++) {
		split_and_insert_2(LM_EMPTY_TILE_WORD, tiles_vec);
	}

	split_and_insert_2(LM_EMPTY_TILE_ACTS_LIKE, acts_like_vec);

	return true;
}

bool HumanReadableMap16::to_map16::try_LM_empty_convert_tiles_only(std::vector<Byte>& tiles_vec, const std::string line, unsigned int expected_tile_number) {
	char buf[256];
	sprintf_s(buf, LM_EMTPY_TILE_FORMAT_NO_NEWLINE, expected_tile_number);
	std::string expected_line = buf;

	if (expected_line != line) {
		return false;
	}

	for (unsigned int i = 0; i != 4; i++) {
		split_and_insert_2(LM_EMPTY_TILE_WORD, tiles_vec);
	}

	return true;
}

HumanReadableMap16::_2Bytes HumanReadableMap16::to_map16::to_bytes(_2Bytes _8x8_tile_number, unsigned int palette, char x_flip, char y_flip, char priority) {
	unsigned int x_bit = x_flip == X_FLIP_ON;
	unsigned int y_bit = y_flip == Y_FLIP_ON;
	unsigned int p_bit = priority == PRIORITY_ON;

	return (y_bit << 15) | (x_bit << 14) | (p_bit << 13) | (palette << 10) | _8x8_tile_number;
}

void HumanReadableMap16::to_map16::convert_full(std::vector<Byte>& tiles_vec, std::vector<Byte>& acts_like_vec, 
	const std::string line, unsigned int expected_tile_number, unsigned int line_number, const fs::path file) {

	verify_full(line, line_number, file, expected_tile_number);  // no verfication yet (dreading it)

	if (try_LM_empty_convert_full(tiles_vec, acts_like_vec, line, expected_tile_number)) {
		return;
	}

	unsigned int _16x16_tile_number, acts_like, _8x8_tile_1, palette_1, _8x8_tile_2, palette_2, 
		_8x8_tile_3, palette_3, _8x8_tile_4, palette_4;
	char x_1, x_2, x_3, x_4, y_1, y_2, y_3, y_4, p_1, p_2, p_3, p_4;

	sscanf_s(line.c_str(), STANDARD_FORMAT,
		&_16x16_tile_number, &acts_like,
		&_8x8_tile_1, &palette_1, &x_1, 1, &y_1, 1, &p_1, 1,
		&_8x8_tile_2, &palette_2, &x_2, 1, &y_2, 1, &p_2, 1,
		&_8x8_tile_3, &palette_3, &x_3, 1, &y_3, 1, &p_3, 1,
		&_8x8_tile_4, &palette_4, &x_4, 1, &y_4, 1, &p_4, 1
	);

	split_and_insert_2(to_bytes(_8x8_tile_1, palette_1, x_1, y_1, p_1), tiles_vec);
	split_and_insert_2(to_bytes(_8x8_tile_2, palette_2, x_2, y_2, p_2), tiles_vec);
	split_and_insert_2(to_bytes(_8x8_tile_3, palette_3, x_3, y_3, p_3), tiles_vec);
	split_and_insert_2(to_bytes(_8x8_tile_4, palette_4, x_4, y_4, p_4), tiles_vec);

	split_and_insert_2(acts_like, acts_like_vec);
}

void HumanReadableMap16::to_map16::verify_full(const std::string line, unsigned int line_number, const fs::path file, 
	unsigned int expected_tile_number) {

	unsigned int curr_char_idx = 0;
	verify_tile_number(line, line_number, file, curr_char_idx, TileFormat::FULL, expected_tile_number);
	
	char potential_shorthand_char;
	try {
		potential_shorthand_char = line.at(curr_char_idx);
	}
	catch (const std::out_of_range&) {
		curr_char_idx = line.size();
		throw TileError("Unexpected end of 16x16 tile while checking for empty tile shorthand symbol",
			file, line_number, line, curr_char_idx, TileFormat::FULL, expected_tile_number);
	}
	if (potential_shorthand_char == LM_EMPTY_TILE_SHORTHAND) {
		if (line.size() == 7) {
			// valid empty tile shorthand, return
			return;
		}
		else {
			throw TileError("Unexpected characters/whitespace after valid empty tile shorthand symbol (~)", 
				file, line_number, line, curr_char_idx, TileFormat::FULL, expected_tile_number);
		}
	}

	verify_acts_like(line, line_number, file, curr_char_idx, TileFormat::FULL, expected_tile_number);
	verify_8x8_tiles(line, line_number, file, curr_char_idx, TileFormat::FULL, expected_tile_number);

	if (curr_char_idx != line.size()) {
		throw TileError("Unexpected characters/whitespace after valid tile specification found",
			file, line_number, line, curr_char_idx, TileFormat::FULL, expected_tile_number);
	}
}

void HumanReadableMap16::to_map16::verify_8x8_tiles(const std::string line, unsigned int line_number, const fs::path file, 
	unsigned int& curr_char_idx, TileFormat tile_format, unsigned int expected_tile_number) {

	std::string opening_bracket = " { ";
	for (const auto c : opening_bracket) {
		char found;
		try {
			found = line.at(curr_char_idx);
		}
		catch (const std::out_of_range&) {
			curr_char_idx = line.size();
			throw TileError("Unexpected end of 16x16 tile while verifying 8x8 tiles", file, line_number, line, curr_char_idx, tile_format, expected_tile_number);
		}

		if (found != c) {
			throw TileError("Unexpected character near opening '{' character", file, line_number, line, curr_char_idx, tile_format, expected_tile_number);
		}
		++curr_char_idx;
	}

	for (int i = 0; i != 4; i++) {
		verify_8x8_tile(line, line_number, file, expected_tile_number, curr_char_idx, tile_format);
		
		if (i == 3) {
			// last tile gets separate whitespace check at the end, including the closing }
			continue;
		}

		std::string whitespace;
		try {
			whitespace = line.substr(curr_char_idx, 2);
		}
		catch (const std::out_of_range&) {
			curr_char_idx = line.size();
			throw TileError("Unexpected end of 16x16 tile while verifying 8x8 tiles", file, line_number, line, curr_char_idx, tile_format, expected_tile_number);
		}

		if (whitespace.size() != 2) {
			curr_char_idx = line.size();
			throw TileError("Unexpected end of 16x16 tile while verifying 8x8 tiles", file, line_number, line, curr_char_idx, tile_format, expected_tile_number);
		}

		if (whitespace != "  ") {
			throw TileError("Incorrect whitespace in 8x8 tiles specification", file, line_number, line, curr_char_idx, tile_format, expected_tile_number);
		}
		curr_char_idx += 2;
	}

	std::string closing_bracket = " }";
	for (const auto c : closing_bracket) {
		char found;
		try {
			found = line.at(curr_char_idx);
		}
		catch (const std::out_of_range&) {
			curr_char_idx = line.size();
			throw  TileError("Unexpected end of 16x16 tile while verifying 8x8 tiles", file, line_number, line, curr_char_idx, tile_format, expected_tile_number);
		}

		if (found != c) {
			throw  TileError("Unexpected character near closing '}' character", file, line_number, line, curr_char_idx, tile_format, expected_tile_number);
		}
		++curr_char_idx;
	}
}

void HumanReadableMap16::to_map16::verify_tile_number(const std::string line, unsigned int line_number, const fs::path file, unsigned int& curr_char_idx,
	TileFormat tile_format, unsigned int expected_tile_number) {
	char tile_number_part[7];
	sprintf_s(tile_number_part, "%04X: ", expected_tile_number);
	std::string tile_number_p = std::string(tile_number_part);

	for (const char c : tile_number_p) {
		char found;
		try {
			found = line.at(curr_char_idx);
		}
		catch (const std::out_of_range&) {
			curr_char_idx = line.size();
			throw  TileError("Unexpected end of 16x16 tile while verifying tile number", file, line_number, line, curr_char_idx, tile_format, expected_tile_number);
		}

		if (found != c) {
			throw  TileError("Unexpected tile number or format", file, line_number, line, curr_char_idx, tile_format, expected_tile_number);
		}
		++curr_char_idx;
	}
}

void HumanReadableMap16::to_map16::verify_acts_like(const std::string line, unsigned int line_number, const fs::path file, unsigned int& curr_char_idx, 
	TileFormat tile_format, unsigned int expected_tile_number) {

	std::string acts_like;
	try {
		acts_like = line.substr(6, 3);
	}
	catch (const std::out_of_range&) {
		curr_char_idx = line.size();
		throw  TileError("Unexpected end of 16x16 tile specification while verifying acts like settings", file, line_number, line, 
			curr_char_idx, tile_format, expected_tile_number);
	}

	if (acts_like.size() != 3) {
		curr_char_idx = line.size();
		throw  TileError("Unexpected end of 16x16 tile specification while verifying acts like settings",
			file, line_number, line, curr_char_idx, tile_format, expected_tile_number
		);
	}

	unsigned int acts_digit_1, acts_digit_2, acts_digit_3;
	unsigned int res = sscanf_s(acts_like.c_str(), "%1X%1X%1X", &acts_digit_1, &acts_digit_2, &acts_digit_3);

	bool is_all_uppercase = std::all_of(acts_like.begin(), acts_like.end(), [](unsigned char c) { return std::isupper(c) || std::isdigit(c); });

	if (res != 3 || !is_all_uppercase) {
		throw  TileError("Acts like setting must be exactly 3 uppercase hexademical digits", file, line_number, 
			line, curr_char_idx, tile_format, expected_tile_number);
	}

	unsigned int acts = (acts_digit_1 << 8) | (acts_digit_2 << 4) | acts_digit_3;

	if (acts >= 0x200) {
		throw  TileError("Unexpected acts like setting >= 0x200", file, line_number, line, 
			curr_char_idx, tile_format, expected_tile_number);
	}

	curr_char_idx += 3;
}

void HumanReadableMap16::to_map16::verify_8x8_tile(const std::string line, unsigned int line_number, const fs::path file,
	unsigned int expected_tile_number, unsigned int& curr_char_idx, TileFormat tile_format) {

	size_t tile_start = curr_char_idx;

	std::string tile_substr, tile_number_substr;
	try {
		tile_substr = line.substr(tile_start, 9);
		tile_number_substr = line.substr(tile_start, 3);
	}
	catch (const std::out_of_range&) {
		curr_char_idx = line.size();
		throw  TileError("Unexpected end of 16x16 tile specification while verifying 8x8 tile specification",
			file, line_number, line, curr_char_idx, tile_format, expected_tile_number
		);
	}

	if (tile_substr.size() != 9) {
		curr_char_idx = line.size();
		throw  TileError("Unexpected end of 16x16 tile specification while verifying 8x8 tile specification",
			file, line_number, line, curr_char_idx, tile_format, expected_tile_number
		);
	}

	bool is_all_uppercase = std::all_of(tile_number_substr.begin(), tile_number_substr.end(), [](unsigned char c) { return std::isupper(c) || std::isdigit(c); });

	if (!is_all_uppercase) {
		throw  TileError("8x8 tile number must be exactly 3 uppercase hexademical digits", file, line_number,
			line, curr_char_idx, tile_format, expected_tile_number);
	}

	unsigned int _8x8_tile_digit_1, _8x8_tile_digit_2, _8x8_tile_digit_3, palette;
	char x, y, p;

	unsigned int res = sscanf_s(tile_substr.c_str(), "%1X%1X%1X %X %c%c%c",
		&_8x8_tile_digit_1, &_8x8_tile_digit_2, &_8x8_tile_digit_3, &palette,
		&x, 1, &y, 1, &p, 1
	);

	if (res != 7) {
		throw  TileError("Incorrect 8x8 tile specification", file, line_number,
			line, curr_char_idx, tile_format, expected_tile_number);
	}

	unsigned int _8x8_tile = (_8x8_tile_digit_1 << 8) | (_8x8_tile_digit_2 << 4) | _8x8_tile_digit_3;

	if (_8x8_tile >= 0x400) {
		throw  TileError("8x8 tile number may not be higher than 0x3FF", file, line_number,
			line, curr_char_idx, tile_format, expected_tile_number);
	}

	curr_char_idx += 4;

	if (palette > 7) {
		throw  TileError("Palette number of 8x8 tile must be between 0 and 7", file, line_number,
			line, curr_char_idx, tile_format, expected_tile_number);
	}
	
	curr_char_idx += 2;

	if (x != X_FLIP_ON && x != X_FLIP_OFF) {
		throw  TileError("Flag for x flip must be either 'x' or '-'", file, line_number,
			line, curr_char_idx, tile_format, expected_tile_number);
	}

	++curr_char_idx;

	if (y != Y_FLIP_ON && y != Y_FLIP_OFF) {
		throw  TileError("Flag for y flip must be either 'y' or '-'", file, line_number,
			line, curr_char_idx, tile_format, expected_tile_number);
	}

	++curr_char_idx;

	if (p != PRIORITY_ON && p != PRIORITY_OFF) {
		throw  TileError("Flag for priority must be either 'p' or '-'", file, line_number,
			line, curr_char_idx, tile_format, expected_tile_number);
	}

	++curr_char_idx;
}

void HumanReadableMap16::to_map16::convert_acts_like_only(std::vector<Byte>& acts_like_vec, const std::string line, 
	unsigned int expected_tile_number, unsigned int line_number, const fs::path file) {
	verify_acts_like_only(line, line_number, file, expected_tile_number);

	unsigned int _16x16_tile_number, acts_like;

	sscanf_s(line.c_str(), NO_TILES_FORMAT,
		&_16x16_tile_number, &acts_like
	);

	split_and_insert_2(acts_like, acts_like_vec);
}

void HumanReadableMap16::to_map16::verify_acts_like_only(const std::string line, unsigned int line_number, const fs::path file, unsigned int expected_tile_number) {
	unsigned int curr_char_idx = 0;
	verify_tile_number(line, line_number, file, curr_char_idx, TileFormat::ACTS_LIKE_ONLY, expected_tile_number);

	char potential_shorthand_char;
	try {
		potential_shorthand_char = line.at(curr_char_idx);
	}
	catch (const std::out_of_range&) {
		curr_char_idx = line.size();
		throw  TileError("Unexpected end of 16x16 tile specification while checking for empty tile shorthand symbol",
			file, line_number, line, curr_char_idx, TileFormat::ACTS_LIKE_ONLY, expected_tile_number);
	}
	if (potential_shorthand_char == LM_EMPTY_TILE_SHORTHAND) {
		throw  TileError("Empty tile shorthand symbol (~) not allowed for acts like only tile specifications",
			file, line_number, line, curr_char_idx, TileFormat::ACTS_LIKE_ONLY, expected_tile_number);
	}

	verify_acts_like(line, line_number, file, curr_char_idx, TileFormat::ACTS_LIKE_ONLY, expected_tile_number);

	if (curr_char_idx != line.size()) {
		throw  TileError("Unexpected characters/whitespace after valid tile specification found",
			file, line_number, line, curr_char_idx, TileFormat::ACTS_LIKE_ONLY, expected_tile_number);
	}
}

void HumanReadableMap16::to_map16::convert_tiles_only(std::vector<Byte>& tiles_vec, const std::string line, 
	unsigned int expected_tile_number, unsigned int line_number, const fs::path file) {
	verify_tiles_only(line, line_number, file, expected_tile_number);

	if (try_LM_empty_convert_tiles_only(tiles_vec, line, expected_tile_number)) {
		return;
	}

	unsigned int _16x16_tile_number, _8x8_tile_1, palette_1, _8x8_tile_2, palette_2,
		_8x8_tile_3, palette_3, _8x8_tile_4, palette_4;
	char x_1, x_2, x_3, x_4, y_1, y_2, y_3, y_4, p_1, p_2, p_3, p_4;

	sscanf_s(line.c_str(), NO_ACTS_FORMAT,
		&_16x16_tile_number,
		&_8x8_tile_1, &palette_1, &x_1, 1, &y_1, 1, &p_1, 1,
		&_8x8_tile_2, &palette_2, &x_2, 1, &y_2, 1, &p_2, 1,
		&_8x8_tile_3, &palette_3, &x_3, 1, &y_3, 1, &p_3, 1,
		&_8x8_tile_4, &palette_4, &x_4, 1, &y_4, 1, &p_4, 1
	);

	split_and_insert_2(to_bytes(_8x8_tile_1, palette_1, x_1, y_1, p_1), tiles_vec);
	split_and_insert_2(to_bytes(_8x8_tile_2, palette_2, x_2, y_2, p_2), tiles_vec);
	split_and_insert_2(to_bytes(_8x8_tile_3, palette_3, x_3, y_3, p_3), tiles_vec);
	split_and_insert_2(to_bytes(_8x8_tile_4, palette_4, x_4, y_4, p_4), tiles_vec);
}

void HumanReadableMap16::to_map16::verify_tiles_only(const std::string line, unsigned int line_number, const fs::path file, unsigned int expected_tile_number) {
	unsigned int curr_char_idx = 0;
	verify_tile_number(line, line_number, file, curr_char_idx, TileFormat::TILES_ONLY, expected_tile_number);

	char potential_shorthand_char;
	try {
		potential_shorthand_char = line.at(curr_char_idx);
	}
	catch (const std::out_of_range&) {
		curr_char_idx = line.size();
		throw  TileError("Unexpected end of tile line while checking for empty tile shorthand symbol", 
			file, line_number, line, curr_char_idx, TileFormat::TILES_ONLY, expected_tile_number);
	}
	if (potential_shorthand_char == LM_EMPTY_TILE_SHORTHAND) {
		if (line.size() == 7) {
			// valid empty tile shorthand, return
			return;
		}
		else {
			throw  TileError("Unexpected characters/whitespace after valid empty tile shorthand symbol (~)",
				file, line_number, line, curr_char_idx, TileFormat::TILES_ONLY, expected_tile_number);
		}
	}

	std::string supposed_to_be_whitespace;
	try {
		supposed_to_be_whitespace = line.substr(6, 3);
	}
	catch (const std::out_of_range&) {
		curr_char_idx = line.size();
		throw  TileError("Unexpected end of tile line while skipping over acts like field", file, line_number, line, curr_char_idx, TileFormat::TILES_ONLY, expected_tile_number);
	}

	if (supposed_to_be_whitespace.size() != 3) {
		curr_char_idx = line.size();
		throw  TileError("Unexpected end of tile line while skipping over acts like field", file, line_number, line, curr_char_idx, TileFormat::TILES_ONLY, expected_tile_number);
	}

	if (supposed_to_be_whitespace != "   ") {
		throw  TileError("Unexpected characters in acts like field that should be empty",
			file, line_number, line, curr_char_idx, TileFormat::TILES_ONLY, expected_tile_number);
	}
	curr_char_idx += 3;

	verify_8x8_tiles(line, line_number, file, curr_char_idx, TileFormat::TILES_ONLY, expected_tile_number);

	if (curr_char_idx != line.size()) {
		throw  TileError("Unexpected characters/whitespace after valid tile specification found",
			file, line_number, line, curr_char_idx, TileFormat::TILES_ONLY, expected_tile_number);
	}
}

std::vector<fs::path> HumanReadableMap16::to_map16::get_sorted_paths(const fs::path directory) {
	std::vector<fs::path> paths{};

	for (const auto& entry : fs::directory_iterator(directory)) {
		paths.push_back(entry);
	}

	std::sort(paths.begin(), paths.end());

	return paths;
}

unsigned int HumanReadableMap16::to_map16::parse_BG_pages(std::vector<Byte>& bg_tiles_vec, unsigned int base_tile_number) {
	if (!fs::exists("global_pages\\BG_pages")) {
		throw FilesystemError("Expected directory appears to be missing", fs::path("global_pages\\BG_pages"));
	}

	const auto sorted_paths = get_sorted_paths("global_pages\\BG_pages");

	unsigned int curr_tile_number = base_tile_number;

	for (const auto& entry : sorted_paths) {
		std::fstream page_file;
		page_file.open(entry);

		std::string line;
		size_t line_no = 1;
		while (std::getline(page_file, line)) {
			convert_tiles_only(bg_tiles_vec, line, curr_tile_number++, line_no++, entry);
		}

		page_file.close();
	}

	return curr_tile_number;
}

unsigned int HumanReadableMap16::to_map16::parse_FG_pages(std::vector<Byte>& fg_tiles_vec, std::vector<Byte>& acts_like_vec, unsigned int base_tile_number) {
	if (!fs::exists("global_pages\\FG_pages")) {
		throw FilesystemError("Expected directory appears to be missing", fs::path("global_pages\\FG_pages"));
	}

	const auto sorted_paths = get_sorted_paths("global_pages\\FG_pages");

	unsigned int curr_tile_number = base_tile_number;

	std::unordered_set<_2Bytes> tileset_group_specific = std::unordered_set<_2Bytes>(TILESET_GROUP_SPECIFIC_TILES.begin(), TILESET_GROUP_SPECIFIC_TILES.end());

	for (const auto& entry : sorted_paths) {
		std::fstream page_file;
		page_file.open(entry);

		std::string line;
		size_t line_no = 1;
		while (std::getline(page_file, line)) {
			if (tileset_group_specific.count(curr_tile_number) == 0) {
				convert_full(fg_tiles_vec, acts_like_vec, line, curr_tile_number++, line_no++, entry);
			}
			else {
				convert_acts_like_only(acts_like_vec, line, curr_tile_number++, line_no++, entry);
				fg_tiles_vec.insert(fg_tiles_vec.end(), { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 });  // tiles are tileset (group) specific, disregard!
			}
		}

		page_file.close();
	}

	return curr_tile_number;
}

unsigned int HumanReadableMap16::to_map16::parse_FG_pages_tileset_specific_page_2(std::vector<Byte>& fg_tiles_vec, std::vector<Byte>& acts_like_vec,
	std::vector<Byte>& tileset_specific_tiles_vec, unsigned int base_tile_number) {
	if (!fs::exists("global_pages\\FG_pages")) {
		throw FilesystemError("Expected directory appears to be missing", fs::path("global_pages\\FG_pages"));
	}

	const auto sorted_paths = get_sorted_paths("global_pages\\FG_pages");

	unsigned int curr_tile_number = base_tile_number;

	std::unordered_set<_2Bytes> tileset_group_specific = std::unordered_set<_2Bytes>(TILESET_GROUP_SPECIFIC_TILES.begin(), TILESET_GROUP_SPECIFIC_TILES.end());

	for (const auto& entry : sorted_paths) {
		std::fstream page_file;
		page_file.open(entry);

		std::string line;
		size_t line_no = 1;
		while (std::getline(page_file, line)) {
			bool not_on_page_2 = curr_tile_number < 0x200 || curr_tile_number >= 0x300;

			if (tileset_group_specific.count(curr_tile_number) == 0 && not_on_page_2) {
				convert_full(fg_tiles_vec, acts_like_vec, line, curr_tile_number++, line_no++, entry);
			}
			else {
				convert_acts_like_only(acts_like_vec, line, curr_tile_number++, line_no++, entry);
				if (not_on_page_2) {
					// tiles are tileset (group) specific on page 0 or 1, will be handled in the tileset_group_specific_tiles!
					fg_tiles_vec.insert(fg_tiles_vec.end(), { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 });  
				}
				else {
					// we're on page 2, LM has a bug (?) that seems to make it so page 2 of the FG block gets written to 
					// tileset E's page 2 if the tileset specific page 2 setting is enabled, so if we don't copy that page 
					// to this section right here it will get overwritten, so I guess we'll do that for now??
					size_t tileset_E_tile_start = 0xE * PAGE_SIZE * _16x16_BYTE_SIZE + (curr_tile_number - 0x200 - 1) * _16x16_BYTE_SIZE;
					size_t tileset_E_tile_end = 0xE * PAGE_SIZE * _16x16_BYTE_SIZE + (curr_tile_number - 0x200) * _16x16_BYTE_SIZE;

					fg_tiles_vec.insert(fg_tiles_vec.end(), 
						tileset_specific_tiles_vec.begin() + tileset_E_tile_start, 
						tileset_specific_tiles_vec.begin() + tileset_E_tile_end
					);
				}
			}
		}

		page_file.close();
	}

	return curr_tile_number;
}

void HumanReadableMap16::to_map16::parse_tileset_group_specific_pages(std::vector<Byte>& tileset_group_specific_tiles_vec, 
	std::vector<Byte>& diagonal_pipe_tiles_vec, const std::vector<Byte>& fg_tiles_vec) {
	if (!fs::exists("tileset_group_specific_tiles")) {
		throw FilesystemError("Expected directory appears to be missing", fs::path("tileset_group_specific_tiles"));
	}

	const auto sorted_paths = get_sorted_paths("tileset_group_specific_tiles");

	std::unordered_set<_2Bytes> tileset_group_specific = std::unordered_set<_2Bytes>(TILESET_GROUP_SPECIFIC_TILES.begin(), TILESET_GROUP_SPECIFIC_TILES.end());

	bool first = true;

	for (const auto& entry : sorted_paths) {
		std::fstream page_file;
		page_file.open(entry);

		std::string line;
		size_t line_no = 1;
		for (unsigned int tile_number = 0; tile_number != PAGE_SIZE * 2; tile_number++) {
			if (tileset_group_specific.count(tile_number) != 0) {
				// if tile is tileset-group-specific, just take the tile spec from the file

				std::getline(page_file, line);
				convert_tiles_only(tileset_group_specific_tiles_vec, line, tile_number, line_no++, entry);
			}
			else {
				// if tile isn't tileset-group-specific, copy its spec from global fg pages vec

				size_t fg_tiles_idx = tile_number * 8;

				tileset_group_specific_tiles_vec.insert(
					tileset_group_specific_tiles_vec.end(), fg_tiles_vec.begin() + fg_tiles_idx, fg_tiles_vec.begin() + fg_tiles_idx + 8
				);
			}
		}

		if (first) {
			// if this is tileset group 0, handle the diagonal pipe tiles

			for (const auto diagonal_pipe_tile_number : DIAGONAL_PIPE_TILES) {
				std::getline(page_file, line);
				convert_tiles_only(diagonal_pipe_tiles_vec, line, diagonal_pipe_tile_number, line_no++, entry);
			}

			first = false;
		}

		page_file.close();
	}
}

void HumanReadableMap16::to_map16::duplicate_tileset_group_specific_pages(std::vector<Byte>& tileset_group_specific_tiles_vec) {
	for (const auto duplicate_tileset_number : DUPLICATE_TILESETS) {
		size_t base_tileset_offset_start = duplicate_tileset_number * PAGE_SIZE * 2 * _16x16_BYTE_SIZE;
		size_t base_tileset_offset_end = (duplicate_tileset_number + 1) * PAGE_SIZE * 2 * _16x16_BYTE_SIZE;
		
		tileset_group_specific_tiles_vec.insert(tileset_group_specific_tiles_vec.end(),
			tileset_group_specific_tiles_vec.begin() + base_tileset_offset_start,
			tileset_group_specific_tiles_vec.begin() + base_tileset_offset_end
		);
	}
}

void HumanReadableMap16::to_map16::parse_tileset_specific_pages(std::vector<Byte>& tileset_specific_tiles_vec) {
	if (!fs::exists("tileset_specific_tiles")) {
		throw FilesystemError("Expected directory appears to be missing", fs::path("tileset_specific_tiles"));
	}

	const auto sorted_paths = get_sorted_paths("tileset_specific_tiles");

	for (const auto& entry : sorted_paths) {
		std::fstream page_file;
		page_file.open(entry);

		unsigned int curr_tile_number = 0x200;

		std::string line;
		size_t line_no = 1;
		while (std::getline(page_file, line)) {
			convert_tiles_only(tileset_specific_tiles_vec, line, curr_tile_number++, line_no++, entry);
		}

		page_file.close();
	}
}

void HumanReadableMap16::to_map16::parse_normal_pipe_tiles(std::vector<Byte>& pipe_tiles_vec) {
	if (!fs::exists("pipe_tiles")) {
		throw FilesystemError("Expected directory appears to be missing", fs::path("pipe_tiles"));
	}

	const auto sorted_paths = get_sorted_paths("pipe_tiles");

	for (const auto& entry : sorted_paths) {
		std::fstream page_file;
		page_file.open(entry);

		std::string line;
		size_t line_no = 1;
		for (const auto tile_number : NORMAL_PIPE_TILES) {
			std::getline(page_file, line);
			convert_tiles_only(pipe_tiles_vec, line, tile_number, line_no++, entry);
		}

		page_file.close();
	}
}

std::vector<HumanReadableMap16::Byte> HumanReadableMap16::to_map16::get_offset_size_vec(size_t header_size, size_t fg_tiles_size,
	size_t bg_tiles_size, size_t acts_like_size, size_t tileset_specific_size, size_t tileset_group_specific_size, size_t normal_pipe_tiles_size, 
	size_t diagonal_pipe_tiles_size) {

	size_t global_pages_size = fg_tiles_size + bg_tiles_size;
	size_t global_and_acts_size = global_pages_size + acts_like_size;

	auto vec = std::vector<OffsetSizePair>{
		OffsetSizePair(header_size + OFFSET_SIZE_TABLE_SIZE, global_pages_size),
		OffsetSizePair(header_size + OFFSET_SIZE_TABLE_SIZE + global_pages_size, acts_like_size),
		OffsetSizePair(header_size + OFFSET_SIZE_TABLE_SIZE, fg_tiles_size),
		OffsetSizePair(header_size + OFFSET_SIZE_TABLE_SIZE + fg_tiles_size, bg_tiles_size),
		OffsetSizePair(header_size + OFFSET_SIZE_TABLE_SIZE + global_and_acts_size, tileset_specific_size),
		OffsetSizePair(header_size + OFFSET_SIZE_TABLE_SIZE + global_and_acts_size + tileset_specific_size, tileset_group_specific_size),
		OffsetSizePair(header_size + OFFSET_SIZE_TABLE_SIZE + global_and_acts_size + tileset_specific_size + tileset_group_specific_size, normal_pipe_tiles_size),
		OffsetSizePair(header_size + OFFSET_SIZE_TABLE_SIZE + global_and_acts_size + tileset_specific_size + tileset_group_specific_size + normal_pipe_tiles_size, diagonal_pipe_tiles_size),
	};

	std::vector<Byte> offset_size_vec{};

	for (const auto& entry : vec) {
		if (vec.size() != 0) {
			split_and_insert_4(entry.first, offset_size_vec);
			split_and_insert_4(entry.second, offset_size_vec);
		}
		else {
			offset_size_vec.insert(offset_size_vec.end(), { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 });
		}
	}

	return offset_size_vec;
}

/*
		_4Bytes file_format_version_number;
		_4Bytes game_id;
		_4Bytes program_version;
		_4Bytes program_id;
		_4Bytes extra_flags;

		_4Bytes offset_size_table_offset;
		_4Bytes offset_size_table_size;

		_4Bytes size_x;
		_4Bytes size_y;

		_4Bytes base_x;
		_4Bytes base_y;

		_4Bytes various_flags_and_info;
*/

std::vector<HumanReadableMap16::Byte> HumanReadableMap16::to_map16::get_header_vec(std::shared_ptr<Header> header) {
	std::vector<Byte> header_vec{};

	for (int i = 0; i != sizeof(header->lm16); i++) {
		header_vec.push_back(header->lm16[i]);
	}
	split_and_insert_2(header->file_format_version_number, header_vec);
	split_and_insert_2(header->game_id, header_vec);
	split_and_insert_2(header->program_version, header_vec);
	split_and_insert_2(header->program_id, header_vec);
	split_and_insert_4(header->extra_flags, header_vec);

	split_and_insert_4(COMMENT_FIELD_OFFSET, header_vec);  // TODO change this once comments are transferred for real
	split_and_insert_4(OFFSET_SIZE_TABLE_SIZE, header_vec);

	split_and_insert_4(header->size_x, header_vec);
	split_and_insert_4(header->size_y, header_vec);

	split_and_insert_4(header->base_x, header_vec);
	split_and_insert_4(header->base_y, header_vec);

	split_and_insert_4(header->various_flags_and_info, header_vec);

	for (unsigned int i = 0; i != 0x14; i++) {
		// insert unused bytes
		header_vec.push_back(0x00);
	}

	return header_vec;
}

std::vector<HumanReadableMap16::Byte> HumanReadableMap16::to_map16::combine(std::vector<Byte> header_vec, std::vector<Byte> offset_size_vec, 
	std::vector<Byte> fg_tiles_vec, std::vector<Byte> bg_tiles_vec, std::vector<Byte> acts_like_vec, std::vector<Byte> tileset_specific_vec, 
	std::vector<Byte> tileset_group_specific_vec, std::vector<Byte> normal_pipe_tiles_vec, std::vector<Byte> diagonal_pipe_tiles_vec) {

	std::vector<Byte> combined{};

	combined.insert(combined.end(), header_vec.begin(), header_vec.end());
	combined.insert(combined.end(), offset_size_vec.begin(), offset_size_vec.end());
	combined.insert(combined.end(), fg_tiles_vec.begin(), fg_tiles_vec.end());
	combined.insert(combined.end(), bg_tiles_vec.begin(), bg_tiles_vec.end());
	combined.insert(combined.end(), acts_like_vec.begin(), acts_like_vec.end());
	combined.insert(combined.end(), tileset_specific_vec.begin(), tileset_specific_vec.end());
	combined.insert(combined.end(), tileset_group_specific_vec.begin(), tileset_group_specific_vec.end());
	combined.insert(combined.end(), normal_pipe_tiles_vec.begin(), normal_pipe_tiles_vec.end());
	combined.insert(combined.end(), diagonal_pipe_tiles_vec.begin(), diagonal_pipe_tiles_vec.end());

	return combined;
}

void HumanReadableMap16::to_map16::convert(const fs::path input_path, const fs::path output_file) {
	fs::path original_working_dir = fs::current_path();

	if (!fs::exists(input_path)) {
		throw FilesystemError("Input path does not appear to exist", input_path);
	}

	if (!fs::is_directory(input_path)) {
		throw FilesystemError("Input path does not appear to be a directory", input_path);
	}

	_wchdir(input_path.c_str());

	verify_header_file("header.txt");

	auto header = parse_header_file("header.txt");

	if (!is_full_game_export(header)) {
		throw HumanMap16Exception("Conversion to non-full-game-export map16 is not (yet?) supported");
	} else {
		if (header->base_x != 0 || header->base_y != 0) {
			throw HumanMap16Exception("Base X and base Y values must be 0 for full game exports");
		}
	}

	std::vector<Byte> fg_tiles_vec{}, bg_tiles_vec{}, acts_like_vec{}, tileset_specific_vec{},
		tileset_group_specific_vec{}, diagonal_pipe_tiles_vec{}, pipe_tiles_vec{};

	if (has_tileset_specific_page_2s(header)) {
		parse_tileset_specific_pages(tileset_specific_vec);
	}

	unsigned int curr_tile_number = 0;

	if (!has_tileset_specific_page_2s(header)) {
		curr_tile_number = parse_FG_pages(fg_tiles_vec, acts_like_vec, curr_tile_number);
	}
	else {
		curr_tile_number = parse_FG_pages_tileset_specific_page_2(fg_tiles_vec, acts_like_vec, tileset_specific_vec, curr_tile_number);
	}
	parse_BG_pages(bg_tiles_vec, curr_tile_number);

	parse_tileset_group_specific_pages(tileset_group_specific_vec, diagonal_pipe_tiles_vec, fg_tiles_vec);

	duplicate_tileset_group_specific_pages(tileset_group_specific_vec);

	parse_normal_pipe_tiles(pipe_tiles_vec);

	auto header_vec = get_header_vec(header);

	auto offset_size_vec = get_offset_size_vec(header_vec.size(), fg_tiles_vec.size(), bg_tiles_vec.size(),
		acts_like_vec.size(), tileset_specific_vec.size(), tileset_group_specific_vec.size(), pipe_tiles_vec.size(), diagonal_pipe_tiles_vec.size());

	const auto combined = combine(header_vec, offset_size_vec, fg_tiles_vec, bg_tiles_vec, acts_like_vec, tileset_specific_vec,
		tileset_group_specific_vec, pipe_tiles_vec, diagonal_pipe_tiles_vec);

	_wchdir(original_working_dir.c_str());

	std::ofstream map16_file(output_file, std::ios::out | std::ios::binary);
	map16_file.write(reinterpret_cast<const char *>(combined.data()), combined.size());
	map16_file.close();
}

/*
#include <iostream>

#define TEST(test_line) \
try { \
	test_line \
} catch (TileError& t) {\
	std::cout << t.get_line() << std::endl << t.get_message() << std::endl << std::endl; \
}

void HumanReadableMap16::to_map16::tests() {
	std::string line1  = "0238: 130 { 182 2 xyp  093 3 --p  239 5 x-p  3FF 3 -y- }";  // succeeds on full
	//                    0238: 238 { 182 2 xyp  093 3 --p  239 5 x-p  3FF 3 -y- }
	std::string line2  = "0238: 238 { 182 2 xyp  093 3 --p  239 5 x-p  3FF 3 -y- }";  // fails on acts like
	std::string line3  = "0238: 130 { 400 2 xyp  093 3 --p  239 5 x-p  3FF 3 -y- }";  // fails on first 8x8 tile number
	std::string line4  = "0238: 130 { 182 8 xyp  093 3 --p  239 5 x-p  3FF 3 -y- }";  // fails on first palette
	std::string line5  = "0238: 130 { 182 2 yxp  093 3 --p  239 5 x-p  3FF 3 -y- }";  // fails on first bits
	std::string line6  = "0238: 130 { 182 2 xyp  093 3 --p  923 5 x-p  3FF 3 -y- }";  // fails on third 8x8 tile number

	TEST(verify_full(line1, 1, "", 0x238););

	TEST(verify_full(line2, 1, "", 0x238););
	TEST(verify_full(line3, 1, "", 0x238););
	TEST(verify_full(line4, 1, "", 0x238););
	TEST(verify_full(line5, 1, "", 0x238););
	TEST(verify_full(line6, 1, "", 0x238););

	std::string line7  = "0238:     { 182 2 xyp  093 3 --p  239 5 x-p  3FF 3 -y- }";  // succeeds on tiles only
	std::string line8  = "0238:     { 482 2 xyp  093 3 --p  239 5 x-p  3FF 3 -y- }";  // fails on first 8x8 tile number
	std::string line9  = "0238:     { 182 8 xyp  093 3 --p  239 5 x-p  3FF 3 -y- }";  // fails on first palette
	std::string line10 = "0238:     { 182 2 yxp  093 3 --p  239 5 x-p  3FF 3 -y- }";  // fails on first bits
	std::string line11 = "0238:     { 182 2 xyp  093 3 --p  923 5 x-p  3FF 3 -y- }";  // fails on third 8x8 tile number

	TEST(verify_tiles_only(line7, 1, "", 0x238););
	TEST(verify_tiles_only(line8, 1, "", 0x238););
	TEST(verify_tiles_only(line9, 1, "", 0x238););
	TEST(verify_tiles_only(line10, 1, "", 0x238););
	TEST(verify_tiles_only(line11, 1, "", 0x238););

	std::string line12 = "0238: 130"; // succeeds on acts like only
	std::string line13 = "0238: 238"; // fails on acts like

	TEST(verify_acts_like_only(line12, 1, "", 0x238););
	TEST(verify_acts_like_only(line13, 1, "", 0x238););

	std::string badac  = "0238: 138 ";

	TEST(verify_acts_like_only(badac, 1, "", 0x238););

	std::string shortc = "0238: ~";

	TEST(verify_full(shortc, 1, "", 0x238););
	TEST(verify_tiles_only(shortc, 1, "", 0x238););
	TEST(verify_acts_like_only(shortc, 1, "", 0x238););

	std::string bad_shortc1 = "0238:  ~";
	std::string bad_shortc2 = "0238: ~ ";

	TEST(verify_full(bad_shortc1, 1, "", 0x238););
	TEST(verify_tiles_only(bad_shortc1, 1, "", 0x238););
	TEST(verify_acts_like_only(bad_shortc1, 1, "", 0x238););

	TEST(verify_full(bad_shortc2, 1, "", 0x238););
	TEST(verify_tiles_only(bad_shortc2, 1, "", 0x238););
	TEST(verify_acts_like_only(bad_shortc2, 1, "", 0x238););

	std::string bad1   = "0238: 130 { 182 2 xyp  093 3 --p  239 5 x-p  3F";
	std::string bad2   = "0238: 130 { 182 2 xyp  093 3 --p  239 5 x-p  3FF 3 -y- } ";
	std::string bad3   = "0238: 130  2 xyp  093 3 --p  239 5 x-p  3FF 3 -y- }";
	std::string bad4   = "023: 130 { 182 2 xyp  093 3 --p  239 5 x-p  3FF 3 -y- }";
	std::string bad5   = "0238: 130 { 182 2 xyp  093 3 --p  239 5 x-p  3FF 3 -y- >";
	std::string bad6   = "0238- 130 { 182 2 xyp  093 3 --p  239 5 x-p  3FF 3 -y- }";
	std::string bad7   = "0238: 130 < 182 2 xyp  093 3 --p  239 5 x-p  3FF 3 -y- }";
	std::string bad8   = "0238: 130 { 182 2 xyp   093 3 --p  239 5 x-p  3FF 3 -y- }";
	std::string bad9   = "0238: 130  { 182 2 xyp  093 3 --p  239 5 x-p  3FF 3 -y- }";

	TEST(verify_full(bad1, 1, "", 0x238););
	TEST(verify_full(bad2, 1, "", 0x238););
	TEST(verify_full(bad3, 1, "", 0x238););
	TEST(verify_full(bad4, 1, "", 0x238););
	TEST(verify_full(bad5, 1, "", 0x238););
	TEST(verify_full(bad6, 1, "", 0x238););
	TEST(verify_full(bad7, 1, "", 0x238););
	TEST(verify_full(bad8, 1, "", 0x238););
	TEST(verify_full(bad9, 1, "", 0x238););

	std::string bad10  = "0238:  a  { 182 2 xyp  093 3 --p  239 5 x-p  3FF 3 -y- }";
	std::string bad11 = "0238:     { 182 2 xyp  093 3 --p a239 5 x-p  3FF 3 -y- }";
	std::string bad12 = "0238:     { 182 2 xyp  093 3 --p a239 5 x-p  3FF 3 -y- }";

	TEST(verify_tiles_only(bad10, 1, "", 0x238););
	TEST(verify_tiles_only(bad11, 1, "", 0x238););
	TEST(verify_tiles_only(bad12, 1, "", 0x238););
}
*/
