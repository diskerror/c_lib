//
// Created by Gemini Agent on 1/12/2026.
//

#ifndef DISKERROR_EXCEPTIONS_H
#define DISKERROR_EXCEPTIONS_H

#include <stdexcept>
#include <string>

namespace Diskerror {

/**
 * Base class for all Diskerror library exceptions.
 */
class Exception : public std::runtime_error {
public:
	explicit Exception(const std::string& message) : std::runtime_error(message) {}
	explicit Exception(const char* message) : std::runtime_error(message) {}
};

// =============================================================================
// File System & I/O Errors
// =============================================================================

class FileError : public Exception {
public:
	using Exception::Exception;
};

class FileNotFound : public FileError {
public:
	explicit FileNotFound(const std::string& path)
		: FileError("File not found: " + path) {}
};

class FileExists : public FileError {
public:
	explicit FileExists(const std::string& path)
		: FileError("File already exists: " + path) {}
};

class NotARegularFile : public FileError {
public:
	explicit NotARegularFile(const std::string& path)
		: FileError("Not a regular file: " + path) {}
};

class FileOpenError : public FileError {
public:
	explicit FileOpenError(const std::string& msg) : FileError(msg) {}
};

// =============================================================================
// Format & Parsing Errors
// =============================================================================

class FormatError : public Exception {
public:
	using Exception::Exception;
};

class InvalidHeader : public FormatError {
public:
	explicit InvalidHeader(const std::string& msg) : FormatError(msg) {}
};

class UnsupportedFormat : public FormatError {
public:
	explicit UnsupportedFormat(const std::string& msg) : FormatError(msg) {}
};

class BitDepthError : public FormatError {
public:
	explicit BitDepthError(const std::string& msg) : FormatError(msg) {}
};

// =============================================================================
// Usage & Logic Errors
// =============================================================================

class UsageError : public Exception {
public:
	using Exception::Exception;
};

class ReservedChunkError : public UsageError {
public:
	explicit ReservedChunkError(const std::string& msg) : UsageError(msg) {}
};

class StopNoError : public Exception {
public:
	using Exception::Exception;
};

} // namespace Diskerror

#endif // DISKERROR_EXCEPTIONS_H
