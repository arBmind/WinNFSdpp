#pragma once

#include "binary.h"

#include <vector>
#include <array>
#include <cstdint>

struct binary_builder_t {
	size_t size() const { return binary_m.size(); }
	size_t offset() const { return offset_m; }

	binary_t build() const { return binary_m; }

	void clear();

	void seek_end();
	void seek_to(size_t offset);

	void append8(uint8_t value);
	void append32(uint32_t value);
	void append64(uint64_t value);

	void append_binary(const uint8_t* binary, size_t);

	// these allow to write enum classes
	template<typename type>
	void append8(type value) { append8(static_cast<uint8_t>(value)); }

	template<typename type>
	void append32(type value) { append32(static_cast<uint32_t>(value)); }

	template<typename type>
	void append64(type value) { append64(static_cast<uint64_t>(value)); }

	template<size_t size>
	void append_binary(const uint8_t (&data)[size]) { append_binary(data, size); }

	template<size_t size>
	void append_binary(const std::array<uint8_t, size> &data) { append_binary(&data[0], size); }

	template<typename binary_t>
	void append_binary(const binary_t& binary) {
	  if (!binary.empty()) { // avoid triggering security checks in debug mode
	      append_binary(
		    reinterpret_cast<const uint8_t*>(&binary[0]),
		  sizeof(typename binary_t::value_type) * binary.size());
	    }
	}

private:
	size_t offset_m = 0;
	binary_t binary_m;
};
