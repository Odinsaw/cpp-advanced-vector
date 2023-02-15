#pragma once
#include <cassert>
#include <cstdlib>
#include <new>
#include <utility>
#include <memory>
#include <algorithm>

template <typename T>
class RawMemory {
public:
	RawMemory() = default;

	explicit RawMemory(size_t capacity)
		: buffer_(Allocate(capacity))
		, capacity_(capacity) {
	}

	RawMemory(const RawMemory&) = delete;
	RawMemory& operator=(const RawMemory& rhs) = delete;
	RawMemory(RawMemory&& other) noexcept
		:buffer_(other.buffer_), capacity_(other.capacity_)
	{
		other.buffer_ = nullptr;
		other.capacity_ = 0;
	}
	RawMemory& operator=(RawMemory&& rhs) noexcept {
		buffer_ = rhs.buffer_;
		capacity_ = rhs.capacity_;
		rhs.buffer_ = nullptr;
		rhs.capacity_ = 0;
		return *this;
	}

	~RawMemory() {
		Deallocate(buffer_);
	}

	T* operator+(size_t offset) noexcept {
		assert(offset <= capacity_);
		return buffer_ + offset;
	}

	const T* operator+(size_t offset) const noexcept {
		return const_cast<RawMemory&>(*this) + offset;
	}

	const T& operator[](size_t index) const noexcept {
		return const_cast<RawMemory&>(*this)[index];
	}

	T& operator[](size_t index) noexcept {
		assert(index < capacity_);
		return buffer_[index];
	}

	void Swap(RawMemory& other) noexcept {
		std::swap(buffer_, other.buffer_);
		std::swap(capacity_, other.capacity_);
	}

	const T* GetAddress() const noexcept {
		return buffer_;
	}

	T* GetAddress() noexcept {
		return buffer_;
	}

	size_t Capacity() const {
		return capacity_;
	}

private:
	static T* Allocate(size_t n) {
		return n != 0 ? static_cast<T*>(operator new(n * sizeof(T))) : nullptr;
	}

	static void Deallocate(T* buf) noexcept {
		operator delete(buf);
	}

	T* buffer_ = nullptr;
	size_t capacity_ = 0;
};

template <typename T>
class Vector {
public:
	Vector() = default;

	explicit Vector(size_t size)
		: data_(size)
		, size_(size)
	{
		std::uninitialized_value_construct_n(begin(), size);
	}

	Vector(const Vector& other)
		: data_(other.size_)
		, size_(other.size_)
	{
		std::uninitialized_copy_n(other.begin(), other.Size(), begin());
	}

	Vector(Vector&& other) noexcept {
		size_ = other.size_;
		data_ = std::move(other.data_);
		other.size_ = 0;
	}

	Vector& operator=(Vector&& rhs) noexcept {
		size_ = rhs.size_;
		data_ = std::move(rhs.data_);
		rhs.size_ = 0;
		return *this;
	}

	void Reserve(size_t new_capacity) {
		if (new_capacity <= data_.Capacity()) {
			return;
		}
		RawMemory<T> new_data(new_capacity);
		if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
			std::uninitialized_move_n(begin(), size_, new_data.GetAddress());
		}
		else {
			std::uninitialized_copy_n(begin(), size_, new_data.GetAddress());
		}
		std::destroy_n(begin(), size_);
		data_.Swap(new_data);
	}

	Vector& operator=(const Vector& rhs) {
		if (this != &rhs) {
			if (rhs.size_ > data_.Capacity()) {
				Vector rhs_copy(rhs);
				Swap(rhs_copy);
			}
			else {
				if (rhs.size_ >= size_) {
					std::copy(rhs.begin(), rhs.begin() + size_, begin());
					std::uninitialized_copy_n(rhs.begin() + size_, rhs.size_ - size_, end());
				}
				else {
					std::copy(rhs.begin(), rhs.end(), begin());
					std::destroy_n(begin() + rhs.size_, size_ - rhs.size_);
				}
			}
			size_ = rhs.size_;
		}
		return *this;
	}
	void Swap(Vector& other) noexcept {
		std::swap(data_, other.data_);
		std::swap(size_, other.size_);
	}

	~Vector() {
		std::destroy_n(begin(), size_);
	}

	using iterator = T*;
	using const_iterator = const T*;

	iterator begin() noexcept {
		return data_.GetAddress();
	}
	iterator end() noexcept {
		return data_.GetAddress() + size_;
	}
	const_iterator begin() const noexcept {
		return data_.GetAddress();
	}
	const_iterator end() const noexcept {
		return data_.GetAddress() + size_;
	}
	const_iterator cbegin() const noexcept {
		return data_.GetAddress();
	}
	const_iterator cend() const noexcept {
		return data_.GetAddress() + size_;
	}

	void Resize(size_t new_size) {
		if (new_size > size_) {
			Reserve(new_size);
			std::uninitialized_value_construct_n(end(), new_size - size_);
		}
		else if (new_size < size_) {
			std::destroy_n(begin() + new_size, size_ - new_size);
		}
		size_ = new_size;
	}

	template <typename...Args>
	T& EmplaceBack(Args&&... args) {
		if (data_.Capacity() >= size_ + 1) {
			PlaceBackWithoutRealloc(std::forward<Args>(args)...);
		}
		else {
			PlaceWithRealloc(cend(), std::forward<Args>(args)...);
		}
		++size_;
		return *(end() - 1);
	}

	void PushBack(const T& value) {
		EmplaceBack(value);
	}

	void PushBack(T&& value) {
		EmplaceBack(std::move(value));
	}

	void PopBack() noexcept {
		std::destroy_n(end() - 1, 1);
		if (size_ > 0) --size_;
	}

	template <typename... Args>
	iterator Emplace(const_iterator pos, Args&&... args) {
		const size_t pos_n = pos - cbegin();
		if (data_.Capacity() > Size()) {
				if (pos != end()) {
					T temp(std::forward<Args>(args)...);
					std::uninitialized_move_n(end() - 1, 1, end());
					std::move_backward(begin() + pos_n, end() - 1, end());
					*(begin() + pos_n) = std::move(temp);
				}
				else {
					PlaceBackWithoutRealloc(std::forward<Args>(args)...);
				}
		}
		else {
			PlaceWithRealloc(cbegin() + pos_n, std::forward<Args>(args)...);
		}
		++size_;
		return begin() + pos_n;
	}

	iterator Erase(const_iterator pos) {
		const size_t pos_n = pos - cbegin();
		std::move(begin() + pos_n + 1, end(), begin() + pos_n);
		std::destroy_at(end() - 1);
		--size_;
		return const_cast<iterator>(pos);
	}

	iterator Insert(const_iterator pos, const T& value) {
		return Emplace(pos, value);
	}

	iterator Insert(const_iterator pos, T&& value) {
		return Emplace(pos, std::move(value));
	}

	size_t Size() const noexcept {
		return size_;
	}

	size_t Capacity() const noexcept {
		return data_.Capacity();
	}

	const T& operator[](size_t index) const noexcept {
		return const_cast<Vector&>(*this)[index];
	}

	T& operator[](size_t index) noexcept {
		assert(index < size_);
		return data_[index];
	}

private:
	void MoveOrCopy(iterator from, size_t num, iterator to) {
		if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
			std::uninitialized_move_n(from, num, to);
		}
		else {
			std::uninitialized_copy_n(from, num, to);
		}
	}

	template <typename...Args>
	void PlaceBackWithoutRealloc(Args&&... args) {
		if (data_.Capacity() >= size_ + 1) {
			//std::construct(data_.GetAddress() + size_, std::forward<Ref>(value));
			new (const_cast<void*>(static_cast<const volatile void*>(end()))) T(std::forward<Args>(args)...);
		}
	}

	template <typename...Args>
	iterator PlaceWithRealloc(const_iterator pos, Args&&... args) {
		const size_t pos_n = pos - cbegin();
		if (data_.Capacity() < size_ + 1) {
			RawMemory<T> new_data(size_ == 0 ? 1 : size_ * 2);
			iterator elem = new (new_data.GetAddress() + pos_n) T(std::forward<Args>(args)...);
			try {
				MoveOrCopy(begin(), pos_n, new_data.GetAddress());
			}
			catch (...) {
				elem -> ~T();
				throw;
			}
			try {
				MoveOrCopy(begin() + pos_n, size_ - pos_n, new_data.GetAddress() + pos_n + 1);
			}
			catch (...) {
				elem -> ~T();
				std::destroy_n(new_data.GetAddress(), pos_n);
				throw;
			}

			std::destroy_n(begin(), size_);
			data_.Swap(new_data);
		}
		return begin() + pos_n;
	}

	static void DestroyN(T* buf, size_t n) noexcept {
		for (size_t i = 0; i != n; ++i) {
			Destroy(buf + i);
		}
	}

	static void CopyConstruct(T* buf, const T& elem) {
		new (buf) T(elem);
	}

	static void Destroy(T* buf) noexcept {
		buf->~T();
	}

	RawMemory<T> data_;
	size_t size_ = 0;
};
