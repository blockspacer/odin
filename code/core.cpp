#include "core.h"

#include <math.h>


Vec_3f vec_3f_create(float32 x, float32 y, float32 z)
{
	Vec_3f v;
	v.x = x;
	v.y = y;
	v.z = z;
	return v;
}

void vec_3f_add(Vec_3f* result, Vec_3f* v)
{
	result->x += v->x;
	result->y += v->y;
	result->z += v->z;
}

void vec_3f_sub(Vec_3f* result, Vec_3f* v)
{
	result->x -= v->x;
	result->y -= v->y;
	result->z -= v->z;
}

void vec_3f_mul(Vec_3f* result, float32 f)
{
	result->x *= f;
	result->y *= f;
	result->z *= f;
}


void matrix_4x4_create_projection(Matrix_4x4* matrix, 
	float32 fov_y, float32 aspect_ratio,
	float32 near_plane, float32 far_plane)
{
	// create projection matrix
	// Note: Vulkan NDC coordinates are top-left corner (-1, -1), z 0-1
	// 1/(tan(fovx/2)*aspect)	0	0				0
	// 0						0	-1/tan(fovy/2)	0
	// 0						-c2	0				c1
	// 0						-1	0				0
	// this is stored column major
	// NDC Z = c1/w + c2
	// c1 = (near*far)/(near-far)
	// c2 = far/(far-near)
	*matrix = {};
	matrix->m11 = 1.0f / (tanf(fov_y * 0.5f) * aspect_ratio);
	matrix->m32 = -(far_plane / (far_plane - near_plane));
	matrix->m42 = -1.0f;
	matrix->m23 = -1.0f / tanf(fov_y * 0.5f);
	matrix->m34 = (near_plane * far_plane) / (near_plane - far_plane);
}


void circular_index_create(Circular_Index* index, uint32 capacity)
{
	index->head = 0;
	index->size = 0;
	index->capacity = capacity;
}

bool32 circular_index_is_full(Circular_Index* index)
{
	return index->size == index->capacity;
}

void circular_index_push(Circular_Index* index)
{
	assert(!circular_index_is_full(index));

	++index->size;
}

void circular_index_pop(Circular_Index* index)
{
	assert(index->size);

	index->head = (index->head + 1) % index->capacity;
	--index->size;
}

uint32 circular_index_tail(Circular_Index* index)
{
	return circular_index_iterator(index, index->size);
}

uint32 circular_index_iterator(Circular_Index* index, uint32 offset)
{
	return (index->head + offset) % index->capacity;
}

void timer_restart(Timer* timer)
{
	QueryPerformanceCounter(&timer->start);
}

Timer timer_create()
{
	Timer timer = {};
	timer_restart(&timer);
	return timer;
}

float32 timer_get_s(Timer* timer)
{
	LARGE_INTEGER now;
	QueryPerformanceCounter(&now);

	return (float32)(now.QuadPart - timer->start.QuadPart) / (float32)globals->clock_frequency.QuadPart;
}

void timer_wait_until(Timer* timer, float32 wait_time_s)
{
	float32 time_taken_s = timer_get_s(timer);

	while (time_taken_s < wait_time_s)
	{
		if (globals->sleep_granularity_was_set)
		{
			DWORD time_to_wait_ms = (DWORD)((wait_time_s - time_taken_s) * 1000);
			if (time_to_wait_ms > 0)
			{// todo(jbr) test this for possible oversleep
				Sleep(time_to_wait_ms);
			}
		}

		time_taken_s = timer_get_s(timer);
	}
}

void memory_allocator_create(Memory_Allocator* allocator, uint8* memory, uint64 size)
{
	allocator->memory = memory;
	allocator->next = memory;
	allocator->bytes_remaining = size;
}

uint8* memory_allocator_alloc(Memory_Allocator* allocator, uint64 size)
{
	assert(allocator->bytes_remaining >= size);
	uint8* mem = allocator->next;
	allocator->next += size;
	return mem;
}

uint8* alloc_permanent(uint64 size)
{
	return memory_allocator_alloc(&globals->permanent_allocator, size);
}

uint8* alloc_temp(uint64 size)
{
	return memory_allocator_alloc(&globals->temp_allocator, size);
}

void globals_init(Log_Function* log_func)
{
	constexpr uint64 c_permanent_memory_size = megabytes(64);
	constexpr uint64 c_temp_memory_size = megabytes(64);
	constexpr uint64 c_total_memory_size = c_permanent_memory_size + c_temp_memory_size;

	uint8* memory = (uint8*)VirtualAlloc((LPVOID)gigabytes(1), c_total_memory_size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);

	// put globals at the start of permanent memory block
	globals = (Globals*)memory;
	
	memory_allocator_create(&globals->permanent_allocator, memory + sizeof(Globals), c_permanent_memory_size - sizeof(Globals));
	memory_allocator_create(&globals->temp_allocator, memory + c_permanent_memory_size, c_temp_memory_size);
	globals->log_function = log_func;
	QueryPerformanceFrequency(&globals->clock_frequency);
	UINT sleep_granularity_ms = 1;
	globals->sleep_granularity_was_set = timeBeginPeriod(sleep_granularity_ms) == TIMERR_NOERROR;
}

void log(const char* format, ...)
{
	assert(globals->log_function);

	va_list args;
	va_start(args, format);
	globals->log_function(format, args);
	va_end(args);
}