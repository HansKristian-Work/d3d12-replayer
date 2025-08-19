/* Copyright (c) 2025 Hans-Kristian Arntzen for Valve Corporation
 * SPDX-License-Identifier: MIT
 */

#pragma once

template <typename T>
class ComPtr
{
public:
	ComPtr() = default;
	~ComPtr() { release(); }

	ComPtr(const ComPtr &other) { *this = other; }
	ComPtr &operator=(const ComPtr &other);
	ComPtr &operator=(ComPtr &&other) noexcept;

	ComPtr(ComPtr &&other) noexcept { *this = std::move(other); }
	ComPtr &operator=(T *ptr_) { release(); ptr = ptr_; return *this; }

	T *operator->() { return ptr; }
	T *get() { return ptr; }
	void **ppv() { release(); return reinterpret_cast<void **>(&ptr); }

	void operator&() = delete;
	explicit operator bool() const { return ptr != nullptr; }

private:
	T *ptr = nullptr;
	void release()
	{
		if (ptr)
			ptr->Release();
		ptr = nullptr;
	}
	ComPtr *self_addr() { return this; }
};

template <typename T>
ComPtr<T> &ComPtr<T>::operator=(const ComPtr &other)
{
	if (this == other.self_addr())
		return *this;
	other.ptr->AddRef();
	release();
	ptr = other.ptr;
	return *this;
}

template <typename T>
ComPtr<T> &ComPtr<T>::operator=(ComPtr &&other) noexcept
{
	if (this == other.self_addr())
		return *this;
	release();
	ptr = other.ptr;
	other.ptr = nullptr;
	return *this;
}

