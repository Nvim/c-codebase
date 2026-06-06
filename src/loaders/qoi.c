#include <loaders/qoi_loader.h>

/* == 8 bits: == */
static uint8_t const QOI_OP_RGB = 0xfe;	 // 0b11111110
static uint8_t const QOI_OP_RGBA = 0xff; // 0b11111111

/* == 2 bits: == */
static uint8_t const QOI_OP_INDEX = 0x00; // 0b00______
static uint8_t const QOI_OP_DIFF = 0x40;  // 0b01______
static uint8_t const QOI_OP_LUMA = 0x80;  // 0b10______
static uint8_t const QOI_OP_RUN = 0xc0;	  // 0b11______

typedef struct pixel {
	uint8_t r;
	uint8_t b;
	uint8_t g;
	uint8_t a;
} pixel;

ARRAY_DEFINE_TYPE(pixel, pixel_array)
QUEUE_DEFINE_TYPE(pixel, pixel_queue)

uint32_t read_u32(uint64_t* offset, uint8_t* data) {
	uint8_t b1 = data[(*offset)++];
	uint8_t b2 = data[(*offset)++];
	uint8_t b3 = data[(*offset)++];
	uint8_t b4 = data[(*offset)++];

	uint32_t ret = 0;
	ret = ret | b1 << 24;
	ret = ret | b2 << 16;
	ret = ret | b3 << 8;
	ret = ret | b4;

	return ret;
}

uint8_t read_u8(uint64_t* offset, uint8_t* data) { return data[(*offset)++]; }

uint64_t get_index(pixel p) {
	return (p.r * 3 + p.g * 5 + p.b * 7 + p.a * 11) % 64;
}

image_read_result load_qoi(arena_t* arena, arena_t* scratch, path filename) {

	image_read_result ret = {0};

	if (!arena || !scratch || !filename.len) {
		LOG_ERR("load_qoi: invalid parameters");
		return ret;
	}

	uint64_t current_offset = 0;
	LOG_INFO("== FILE: " STR_FMT " ==", STR_FMT_ARGS(filename));

	fs_file_read_result r = fs_read_all_file(arena, filename);
	if (!r.ok) {
		LOG_ERR("load_qoi: couldn't read file");
		return ret;
	}

	current_offset = 4;
	string magic = (string){.len = current_offset, .str = r.str.str};
	LOG_INFO("* MAGIC: " STR_FMT, STR_FMT_ARGS(magic));

	uint8_t* raw = u8_array_begin(r.bytes);

	uint32_t width = read_u32(&current_offset, raw);
	uint32_t height = read_u32(&current_offset, raw);
	uint8_t channels = read_u8(&current_offset, raw);
	uint8_t colorspace = read_u8(&current_offset, raw);

	LOG_INFO("* Resolution: (%ux%u)", width, height);
	LOG_INFO("* Channels: %u", channels);
	LOG_INFO("* Color Space: %u", colorspace);
	LOG_INFO("* offset: %lu", current_offset);

	if (channels != 4) {
		LOG_WARN("only 4 channels images are supported for now");
		return ret;
	}

	pixel previous_pixels[64] = {0};
	pixel_array pixel_buffer = pixel_array_create(arena, width * height);

	uint64_t write_index = 0; // increment after writing every pixel
	pixel p = {0, 0, 0, 0xff};
	int run = 0;
	while (write_index < width * height) {
		uint8_t byte = read_u8(&current_offset, raw);

		if (run > 0) {
			run--;
		} else {
			// try 8 bit operations first:
			if (byte == QOI_OP_RGB) {
				LOG_DEBUG("RGB");
				p.r = read_u8(&current_offset, raw);
				p.g = read_u8(&current_offset, raw);
				p.b = read_u8(&current_offset, raw);
				p.a = 0xff;
			} else if (byte == QOI_OP_RGBA) {
				LOG_DEBUG("RGBA");
				p.r = read_u8(&current_offset, raw);
				p.g = read_u8(&current_offset, raw);
				p.b = read_u8(&current_offset, raw);
				p.a = read_u8(&current_offset, raw);
			} else if ((byte & 0xc0) == QOI_OP_INDEX) {
				LOG_DEBUG("INDEX");
				uint8_t index = byte | 0x3f;
				p = previous_pixels[index];
			} else if ((byte & 0xc0) == QOI_OP_DIFF) {
				LOG_DEBUG("DIFF");
				int8_t dr = (byte & 0x30);
				int8_t dg = (byte & 0x0c);
				int8_t db = (byte & 0x03);

				p.r += (dr - 2);
				p.g += (dg - 2);
				p.b += (db - 2);
				p.a += p.a; // alpha doesn't change
			} else if ((byte & 0xc0) == QOI_OP_LUMA) {
				LOG_DEBUG("LUMA");
				int8_t next_byte = read_u8(&current_offset, raw);

				int8_t dg = (int8_t)(byte & 0x3f) - 32;
				int8_t drdg = (next_byte & 0xf0);
				int8_t dbdg = (next_byte & 0x0f);

				p.g += dg;
				p.r += dg - 8 + drdg;
				p.b += (dg - 8) + dbdg;
				p.a = p.a; // alpha doesn't change
			} else if ((byte & 0xc0) == QOI_OP_RUN) {
				uint8_t run_length = (byte & 0x3f);
				LOG_DEBUG("RUN: %u", run_length);
				ASSERT(run_length != 63 && run_length != 64);
				run = run_length;
			} else if (byte == 0x00) {
				LOG_DEBUG("0x00 -- final loop");
				for (uint8_t i = 0; i < 6; i++) {
					uint8_t b = read_u8(&current_offset, raw);
					ASSERT(b == 0x00);
				}
				uint8_t b = read_u8(&current_offset, raw);
				ASSERT(b == 0x01);
				LOG_INFO("end of stream");
				break;
			} else {
				LOG_ERR("BAD BYTE: 0x%x (%u)", byte, byte);
				return ret;
			}
		}

		pixel_array_set(pixel_buffer, write_index++, &p);
		previous_pixels[get_index(p)] = p;
	}

	LOG_INFO("%lu pixels", write_index);

	u8_array ret_arr = u8_array_create(arena, width * height * channels);
	uint64_t byte_idx = 0;

	pixel* pixel_buffer_base = pixel_array_begin(pixel_buffer);
	for (uint32_t j = 0; j < height; ++j) {
		for (uint32_t i = 0; i < width; ++i) {
			uint32_t pixel_idx = (j * width) + i;
			u8_array_set(ret_arr, byte_idx++, &pixel_buffer_base[pixel_idx].r);
			u8_array_set(ret_arr, byte_idx++, &pixel_buffer_base[pixel_idx].g);
			u8_array_set(ret_arr, byte_idx++, &pixel_buffer_base[pixel_idx].b);
			u8_array_set(ret_arr, byte_idx++, &pixel_buffer_base[pixel_idx].a);

			// printf("{%u %u %u %u}", pixel_buffer_base[pixel_idx].r,
			// 	   pixel_buffer_base[pixel_idx].g,
			// 	   pixel_buffer_base[pixel_idx].b,
			// 	   pixel_buffer_base[pixel_idx].a);
		}
		// printf("\n");
	}

	{
		ret.ok = true;
		ret.height = height;
		ret.width = width;
		ret.channels = channels;
		ret.bytes = ret_arr;
	}

	return ret;
}
