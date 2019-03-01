#include "core.h"

#include <cmath>
#include <cstdio>


Vec_3f vec_3f(float32 x, float32 y, float32 z)
{
	Vec_3f v;
	v.x = x;
	v.y = y;
	v.z = z;
	return v;
}

Vec_3f vec_3f_add(Vec_3f a, Vec_3f b)
{
	return vec_3f(a.x + b.x, a.y + b.y, a.z + b.z);
}

Vec_3f vec_3f_sub(Vec_3f a, Vec_3f b)
{
	return vec_3f(a.x - b.x, a.y - b.y, a.z - b.z);
}

Vec_3f vec_3f_mul(Vec_3f v, float32 f)
{
	return vec_3f(v.x * f, v.y * f, v.z * f);
}

Vec_3f vec_3f_normalised(Vec_3f v)
{
	float32 length_sq = (v.x * v.x) + (v.y * v.y) + (v.z * v.z);
	if (length_sq > 0.0f)
	{
		float32 inv_length = 1 / (float32)sqrt(length_sq);
		return vec_3f(v.x * inv_length, v.y * inv_length, v.z * inv_length);
	}

	return v;
}

float vec_3f_dot(Vec_3f a, Vec_3f b)
{
	return (a.x * b.x) + (a.y * b.y) + (a.z * b.z);
}

Vec_3f vec_3f_cross(Vec_3f a, Vec_3f b)
{
	return vec_3f((a.y * b.z) - (a.z * b.y), (a.z * b.x) - (a.x * b.z), (a.x * b.y) - (a.y * b.x));
}


void matrix_4x4_identity(Matrix_4x4* matrix)
{
	matrix->m11 = 1.0f;
	matrix->m21 = 0.0f;
	matrix->m31 = 0.0f;
	matrix->m41 = 0.0f;
	matrix->m12 = 0.0f;
	matrix->m22 = 1.0f;
	matrix->m32 = 0.0f;
	matrix->m42 = 0.0f;
	matrix->m13 = 0.0f;
	matrix->m23 = 0.0f;
	matrix->m33 = 1.0f;
	matrix->m43 = 0.0f;
	matrix->m14 = 0.0f;
	matrix->m24 = 0.0f;
	matrix->m34 = 0.0f;
	matrix->m44 = 1.0f;
}

void matrix_4x4_projection(Matrix_4x4* matrix, 
	float32 fov_y, float32 aspect_ratio,
	float32 near_plane, float32 far_plane)
{
	// create projection matrix
	// Note: Vulkan NDC coordinates are top-left corner (-1, -1), z 0-1
	// 1/(tan(fovx/2)*aspect)	0	0				0
	// 0						0	-1/tan(fovy/2)	0
	// 0						c2	0				c1
	// 0						1	0				0
	// this is stored column major
	// NDC Z = c1/w + c2
	// c1 = (near*far)/(near-far)
	// c2 = far/(far-near)
	*matrix = {};
	matrix->m11 = 1.0f / (tanf(fov_y * 0.5f) * aspect_ratio);
	matrix->m32 = (far_plane / (far_plane - near_plane));
	matrix->m42 = 1.0f;
	matrix->m23 = -1.0f / tanf(fov_y * 0.5f);
	matrix->m34 = (near_plane * far_plane) / (near_plane - far_plane);
}

void matrix_4x4_translation(Matrix_4x4* matrix, float32 x, float32 y, float32 z)
{
	matrix->m11 = 1.0f;
	matrix->m21 = 0.0f;
	matrix->m31 = 0.0f;
	matrix->m41 = 0.0f;
	matrix->m12 = 0.0f;
	matrix->m22 = 1.0f;
	matrix->m32 = 0.0f;
	matrix->m42 = 0.0f;
	matrix->m13 = 0.0f;
	matrix->m23 = 0.0f;
	matrix->m33 = 1.0f;
	matrix->m43 = 0.0f;
	matrix->m14 = x;
	matrix->m24 = y;
	matrix->m34 = z;
	matrix->m44 = 1.0f;
}

void matrix_4x4_translation(Matrix_4x4* matrix, Vec_3f translation)
{
	matrix_4x4_translation(matrix, translation.x, translation.y, translation.z);
}

void matrix_4x4_rotation_z(Matrix_4x4* matrix, float32 r)
{
	float32 cr = cosf(r);
	float32 sr = sinf(r);
	matrix->m11 = cr;
	matrix->m21 = sr;
	matrix->m31 = 0.0f;
	matrix->m41 = 0.0f;
	matrix->m12 = -sr;
	matrix->m22 = cr;
	matrix->m32 = 0.0f;
	matrix->m42 = 0.0f;
	matrix->m13 = 0.0f;
	matrix->m23 = 0.0f;
	matrix->m33 = 1.0f;
	matrix->m43 = 0.0f;
	matrix->m14 = 0.0f;
	matrix->m24 = 0.0f;
	matrix->m34 = 0.0f;
	matrix->m44 = 1.0f;
}

void matrix_4x4_mul(Matrix_4x4* result, Matrix_4x4* a, Matrix_4x4* b)
{
	assert(result != a && result != b);
	result->m11 = (a->m11 * b->m11) + (a->m12 * b->m21) + (a->m13 * b->m31) + (a->m14 * b->m41);
	result->m21 = (a->m21 * b->m11) + (a->m22 * b->m21) + (a->m23 * b->m31) + (a->m24 * b->m41);
	result->m31 = (a->m31 * b->m11) + (a->m32 * b->m21) + (a->m33 * b->m31) + (a->m34 * b->m41);
	result->m41 = (a->m41 * b->m11) + (a->m42 * b->m21) + (a->m43 * b->m31) + (a->m44 * b->m41);
	result->m12 = (a->m11 * b->m12) + (a->m12 * b->m22) + (a->m13 * b->m32) + (a->m14 * b->m42);
	result->m22 = (a->m21 * b->m12) + (a->m22 * b->m22) + (a->m23 * b->m32) + (a->m24 * b->m42);
	result->m32 = (a->m31 * b->m12) + (a->m32 * b->m22) + (a->m33 * b->m32) + (a->m34 * b->m42);
	result->m42 = (a->m41 * b->m12) + (a->m42 * b->m22) + (a->m43 * b->m32) + (a->m44 * b->m42);
	result->m13 = (a->m11 * b->m13) + (a->m12 * b->m23) + (a->m13 * b->m33) + (a->m14 * b->m43);
	result->m23 = (a->m21 * b->m13) + (a->m22 * b->m23) + (a->m23 * b->m33) + (a->m24 * b->m43);
	result->m33 = (a->m31 * b->m13) + (a->m32 * b->m23) + (a->m33 * b->m33) + (a->m34 * b->m43);
	result->m43 = (a->m41 * b->m13) + (a->m42 * b->m23) + (a->m43 * b->m33) + (a->m44 * b->m43);
	result->m14 = (a->m11 * b->m14) + (a->m12 * b->m24) + (a->m13 * b->m34) + (a->m14 * b->m44);
	result->m24 = (a->m21 * b->m14) + (a->m22 * b->m24) + (a->m23 * b->m34) + (a->m24 * b->m44);
	result->m34 = (a->m31 * b->m14) + (a->m32 * b->m24) + (a->m33 * b->m34) + (a->m34 * b->m44);
	result->m44 = (a->m41 * b->m14) + (a->m42 * b->m24) + (a->m43 * b->m34) + (a->m44 * b->m44);
}

void matrix_4x4_lookat(Matrix_4x4* matrix, Vec_3f position, Vec_3f target, Vec_3f up)
{
	Matrix_4x4 translation;
	matrix_4x4_translation(&translation, vec_3f_mul(position, -1.0f));

	Vec_3f view_forward = vec_3f_normalised(vec_3f_sub(target, position));
	Vec_3f project_up_onto_forward = vec_3f_mul(view_forward, vec_3f_dot(up, view_forward));
	Vec_3f view_up = vec_3f_normalised(vec_3f_sub(up, project_up_onto_forward));
	Vec_3f view_right = vec_3f_cross(view_forward, view_up);

	Matrix_4x4 rotation;
	rotation.m11 = view_right.x;
	rotation.m21 = view_forward.x;
	rotation.m31 = view_up.x;
	rotation.m41 = 0.0f;
	rotation.m12 = view_right.y;
	rotation.m22 = view_forward.y;
	rotation.m32 = view_up.y;
	rotation.m42 = 0.0f;
	rotation.m13 = view_right.z;
	rotation.m23 = view_forward.z;
	rotation.m33 = view_up.z;
	rotation.m43 = 0.0f;
	rotation.m14 = 0.0f;
	rotation.m24 = 0.0f;
	rotation.m34 = 0.0f;
	rotation.m44 = 1.0f;

	matrix_4x4_mul(matrix, &rotation, &translation);
}


Timer timer()
{
	Timer timer = {};
	QueryPerformanceFrequency(&timer.frequency);
	QueryPerformanceCounter(&timer.start);
	return timer;
}

float32 timer_get_s(Timer* timer)
{
	LARGE_INTEGER now;
	QueryPerformanceCounter(&now);

	return (float32)(now.QuadPart - timer->start.QuadPart) / (float32)timer->frequency.QuadPart;
}

void timer_wait_until(Timer* timer, float32 wait_time_s, bool sleep_granularity_is_set)
{
	float32 time_taken_s = timer_get_s(timer);

	while (time_taken_s < wait_time_s)
	{
		if (sleep_granularity_is_set)
		{
			DWORD time_to_wait_ms = (DWORD)((wait_time_s - time_taken_s) * 1000);
			if (time_to_wait_ms > 1) // Sleep frequently oversleeps by 1ms, so spin for everything smaller than 2
			{
				Sleep(time_to_wait_ms);
			}
		}

		time_taken_s = timer_get_s(timer);
	}
}

void timer_shift_start(Timer* timer, float32 accumulate_s)
{
	timer->start.QuadPart += (LONGLONG)(timer->frequency.QuadPart * accumulate_s);
}

void linear_allocator_create(Linear_Allocator* allocator, uint64 size)
{
	allocator->memory = new uint8[size];
	allocator->next = allocator->memory;
	allocator->bytes_remaining = size;
}

void linear_allocator_create_sub_allocator(Linear_Allocator* allocator, Linear_Allocator* sub_allocator, uint64 size)
{
	sub_allocator->memory = linear_allocator_alloc(allocator, size);
	sub_allocator->next = sub_allocator->memory;
	sub_allocator->bytes_remaining = size;
}

uint8* linear_allocator_alloc(Linear_Allocator* allocator, uint64 size)
{
	assert(allocator->bytes_remaining >= size);
	uint8* mem = allocator->next;
	allocator->next += size;
	allocator->bytes_remaining -= size;
	return mem;
}


void log(const char* format, ...)
{
	char buffer[512];

	va_list args;
	va_start(args, format);
	vsnprintf(buffer, sizeof(buffer), format, args);
	va_end(args);

	OutputDebugStringA(buffer);
}