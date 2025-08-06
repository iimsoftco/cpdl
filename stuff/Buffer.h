#pragma once
#include <initializer_list>
#include <vector>
#include <string>
#include <cstdint>
#include <cstring>
#include <type_traits>
#include <stdexcept>
#include <utility>

template <typename T>
struct is_container {
private:
	template <typename C>
	static auto test(int) -> decltype(
		std::declval<const C&>().data(),
		std::declval<const C&>().size(),
		typename C::value_type{},
		std::true_type{});

	template <typename>
	static std::false_type test(...);
public:
	static constexpr bool value = decltype(test<T>(0))::value;
};

template <typename T>
struct is_std_basic_string : std::false_type {};

template <typename CharT, typename Traits, typename Alloc>
struct is_std_basic_string<std::basic_string<CharT, Traits, Alloc>> : std::true_type {};

template <typename T>
struct is_cstring_like {
private:
	using U = typename std::decay<T>::type;
	using Pointee = typename std::remove_const<typename std::remove_pointer<U>::type>::type;

public:
	static constexpr bool value =
		std::is_pointer<U>::value &&
		(
			std::is_same<Pointee, char>::value ||
			std::is_same<Pointee, wchar_t>::value ||
			std::is_same<Pointee, char16_t>::value ||
			std::is_same<Pointee, char32_t>::value
		);
};

class Buffer : public std::vector<uint8_t>
{
public:
	Buffer() = default;
	Buffer(const Buffer& buffer) = default;
	Buffer(Buffer&& buffer) noexcept = default;
	Buffer(const std::vector<uint8_t>& vec) : std::vector<uint8_t>(vec) {}
	Buffer(std::vector<uint8_t>&& vec) noexcept : std::vector<uint8_t>(std::move(vec)) {}

	Buffer& operator=(const Buffer& buffer) = default;
	Buffer& operator=(Buffer&& buffer) noexcept = default;
	Buffer& operator=(const std::vector<uint8_t>& vec) {
		std::vector<uint8_t>::operator=(vec);
		return *this;
	}
	Buffer& operator=(std::vector<uint8_t>&& vec) noexcept {
		std::vector<uint8_t>::operator=(std::move(vec));
		return *this;
	}

	template <typename... T>
	static Buffer make(const T&... vals) {
		Buffer buf;
		int dummy[] = { (buf.write(vals), 0)... };
		(void)dummy;
		return buf;
	}

	template <typename T>
	static Buffer make(const std::initializer_list<T>& list) {
		Buffer buf;
		return buf.write(list);
	}

	void skip(size_t size) { bytesRead += size; }
	void seek(size_t offset) { bytesRead = offset; }
	Buffer::iterator current_pos() { return this->begin() + bytesRead; }
	Buffer::const_iterator current_pos() const { return this->begin() + bytesRead; }
	Buffer::const_iterator ccurrent_pos() const { return this->begin() + bytesRead; }
	size_t current_offset() const { return bytesRead; }
	size_t last_peek_size() const { return lastPeekSize; }

	Buffer& write_from(const void* source, size_t size) {
		this->insert(this->end(), static_cast<const uint8_t*>(source), static_cast<const uint8_t*>(source) + size);
		return *this;
	}

	template <typename T>
	Buffer& write(const T& value) {
		return this->write_from(this->get_val_ptr(value), this->get_val_size_bytes(value));
	}

	template <typename... T>
	Buffer& write(const T&... vals) {
		int dummy[] = { (this->write(vals), 0)... };
		(void)dummy;
		return *this;
	}

	template <typename T>
	Buffer& write(const std::initializer_list<T>& list) {
		return this->write_from(&*list.begin(), list.size() * sizeof(T));
	}

	template <typename SizeType, typename T>
	typename std::enable_if<std::is_integral<SizeType>::value, Buffer&>::type
	write(const T& value) {
		this->write(static_cast<SizeType>(this->get_val_size(value)));
		this->write(value);
		return *this;
	}

	template <typename T>
	T ptr_to(size_t offset) {
		return reinterpret_cast<T>(this->data() + offset);
	}

	template <typename T>
	T ptr_to(size_t offset) const {
		return reinterpret_cast<T>(this->data() + offset);
	}

	template <typename T = int>
	T peek_at(size_t offset) const {
		this->check_offset(offset + sizeof(T));
		this->lastPeekSize = sizeof(T);
		return *this->ptr_to<const T*>(offset);
	}

	template <typename ContainerT, typename SizeType>
	typename std::enable_if<
		std::is_integral<SizeType>::value &&
		(std::is_same<ContainerT, Buffer>::value || is_std_basic_string<ContainerT>::value),
		ContainerT
	>::type
	peek_at(size_t offset) const {
		auto contSize = this->peek<SizeType>();
		return this->peek_at<ContainerT>(offset + sizeof(SizeType), contSize);
	}

	template <typename ContainerT>
	typename std::enable_if<
		(std::is_same<ContainerT, Buffer>::value || is_std_basic_string<ContainerT>::value),
		ContainerT
	>::type
	peek_at(size_t offset, size_t size) const {
		size_t bytesSize = size * sizeof(typename ContainerT::value_type);
		this->check_offset(offset + bytesSize);
		ContainerT cont;
		cont.resize(size);
		std::memcpy(cont.data(), this->data() + offset, this->lastPeekSize = bytesSize);
		return cont;
	}

	template <typename String>
	typename std::enable_if<is_std_basic_string<String>::value, String>::type
	peek_at(size_t offset) const {
		String string = this->ptr_to<const typename String::value_type*>(this->bytesRead);
		this->lastPeekSize = this->get_val_size_bytes(string) + sizeof(typename String::value_type);
		return string;
	}

	template <typename T = int>
	T peek() const {
		return this->peek_at<T>(this->bytesRead);
	}

	template <typename ContainerT, typename SizeType>
	typename std::enable_if<
		std::is_integral<SizeType>::value &&
		(std::is_same<ContainerT, Buffer>::value || is_std_basic_string<ContainerT>::value),
		ContainerT
	>::type
	peek() const {
		return this->peek_at<ContainerT, SizeType>(this->bytesRead);
	}

	template <typename ContainerT>
	typename std::enable_if<
		(std::is_same<ContainerT, Buffer>::value || is_std_basic_string<ContainerT>::value),
		ContainerT
	>::type
	peek(size_t size) const {
		return this->peek_at<ContainerT>(this->bytesRead, size);
	}

	template <typename T = int>
	T read(size_t& offset) const {
		auto value = this->peek_at<T>(offset);
		offset += this->lastPeekSize;
		return value;
	}

	template <typename ContainerT, typename SizeType>
	typename std::enable_if<
		std::is_integral<SizeType>::value &&
		(std::is_same<ContainerT, Buffer>::value || is_std_basic_string<ContainerT>::value),
		ContainerT
	>::type
	read(size_t& offset) const {
		auto bufSize = this->read<SizeType>(offset);
		return this->read<ContainerT>(bufSize, offset);
	}

	template <typename ContainerT>
	typename std::enable_if<
		(std::is_same<ContainerT, Buffer>::value || is_std_basic_string<ContainerT>::value),
		ContainerT
	>::type
	read(size_t size, size_t& offset) const {
		auto value = this->peek_at<ContainerT>(offset, size);
		offset += this->lastPeekSize;
		return value;
	}

	template <typename T = int>
	T read() {
		return static_cast<const Buffer*>(this)->read<T>(this->bytesRead);
	}

	template <typename ContainerT, typename SizeType>
	typename std::enable_if<
		std::is_integral<SizeType>::value &&
		(std::is_same<ContainerT, Buffer>::value || is_std_basic_string<ContainerT>::value),
		ContainerT
	>::type
	read() {
		return static_cast<const Buffer*>(this)->read<ContainerT, SizeType>(this->bytesRead);
	}

	template <typename ContainerT>
	typename std::enable_if<
		(std::is_same<ContainerT, Buffer>::value || is_std_basic_string<ContainerT>::value),
		ContainerT
	>::type
	read(size_t size) {
		return static_cast<const Buffer*>(this)->read<ContainerT>(size, this->bytesRead);
	}

private:
	void check_offset(size_t offset) const {
		if (offset > this->size())
			throw std::out_of_range("Buffer read out of bounds");
	}

	template <typename T>
	const void* get_val_ptr(const T& value) const {
		return &value;
	}

	template <typename T>
	typename std::enable_if<is_container<T>::value, const void*>::type
	get_val_ptr(const T& value) const {
		return value.data();
	}

	template <typename T>
	typename std::enable_if<is_cstring_like<T>::value, const void*>::type
	get_val_ptr(const T& value) const {
		return value;
	}

	template <typename T>
	size_t get_val_size(const T& value) const {
		return sizeof(T);
	}

	template <typename T>
	typename std::enable_if<is_container<T>::value, size_t>::type
	get_val_size(const T& value) const {
		return value.size();
	}

	template <typename T>
	typename std::enable_if<is_cstring_like<T>::value, size_t>::type
	get_val_size(const T& value) const {
		using CharType = typename std::remove_const<typename std::remove_pointer<typename std::decay<T>::type>::type>::type;
		return std::char_traits<CharType>::length(value) + 1;
	}

	template <typename T>
	size_t get_val_size_bytes(const T& value) const {
		return this->get_val_size(value);
	}

	template <typename T>
	typename std::enable_if<is_container<T>::value, size_t>::type
	get_val_size_bytes(const T& value) const {
		return this->get_val_size(value) * sizeof(typename T::value_type);
	}

	template <typename T>
	typename std::enable_if<is_cstring_like<T>::value, size_t>::type
	get_val_size_bytes(const T& value) const {
		using CharType = typename std::remove_const<typename std::remove_pointer<typename std::decay<T>::type>::type>::type;
		return this->get_val_size(value) * sizeof(CharType);
	}

	size_t bytesRead = 0;
	mutable size_t lastPeekSize = 0;
};
