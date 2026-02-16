#pragma once

#include <exception>
#include <string>

class HumanMap16Exception : public std::exception {
	private:
		std::string _message;

	public:
		HumanMap16Exception(std::string message) {
			_message = message;
		}

		const std::string get_message() const {
			return _message;
		}

		virtual std::string get_detailed_error_message() const {
			return _message;
		}
};
