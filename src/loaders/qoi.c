#include <loaders/qoi_loader.h>

/* == 8 bits: == */
static uint8_t const QOI_OP_RGB = 0xfe;	 // 0b11111110
static uint8_t const QOI_OP_RGBA = 0xff; // 0b11111111

/* == 2 bits: == */
static uint8_t const QOI_OP_INDEX = 0x00; // 0b00______
static uint8_t const QOI_OP_DIFF = 0x40;  // 0b01______
static uint8_t const QOI_OP_LUMA = 0x80;  // 0b10______
static uint8_t const QOI_OP_RUN = 0xc0;	  // 0b11______

static uint64_t const HEADER_SIZE = 14;
static uint8_t const END_SIZE = 8 * sizeof(uint8_t);

typedef struct pixel {
	uint8_t r;
	uint8_t g;
	uint8_t b;
	uint8_t a;
} pixel;

ARRAY_DEFINE_TYPE(pixel, pixel_array)

uint32_t _read_u32(uint64_t* offset, uint8_t* data) {
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

static uint8_t _read_u8(uint64_t* offset, uint8_t* data) {
	return data[(*offset)++];
}

static uint64_t _get_index(pixel p) {
	return (p.r * 3 + p.g * 5 + p.b * 7 + p.a * 11) % 64;
}

static bool _magic_valid(string magic) {
	return string_compare(magic, str("qoif"));
}

static bool _channels_valid(uint8_t ch) { return ch == 3 || ch == 4; }

static bool _colorspace_valid(uint8_t c) { return c == 0 || c == 1; }

image_read_result load_qoi(arena_t* arena, arena_t* scratch, path filename,
						   bool force_4_channels) {

	image_read_result ret = {0};

	if (!arena || !scratch || !filename.len) {
		LOG_ERR("load_qoi: invalid parameters");
		return ret;
	}

	fs_file_read_result r = fs_read_all_file(scratch, filename);
	if (!r.ok) {
		LOG_ERR("load_qoi: couldn't read file");
		return ret;
	}

	uint64_t file_size = u8_array_size(r.bytes);
	if (file_size < HEADER_SIZE + END_SIZE) {
		LOG_ERR("load_qoi: file is too small");
		return ret;
	}

	uint64_t current_offset = 4;

	string magic = (string){.len = current_offset, .str = r.str.str};
	if (!_magic_valid(magic)) {
		LOG_ERR("load_qoi: file isn't a QOI file");
		return ret;
	}

	uint8_t* raw = u8_array_begin(r.bytes);
	uint32_t width = _read_u32(&current_offset, raw);
	uint32_t height = _read_u32(&current_offset, raw);
	uint8_t channels = _read_u8(&current_offset, raw);
	uint8_t colorspace = _read_u8(&current_offset, raw);

	LOG_INFO("load_qoi: " STR_FMT " (%ux%u) - %u channels, %d colorspace",
			 STR_FMT_ARGS(filename), width, height, channels, colorspace);

	if (!width || !height || !_channels_valid(channels) ||
		!_colorspace_valid(colorspace)) {
		LOG_ERR("load_qoi: invalid header");
		return ret;
	}

	if (channels == 3 && force_4_channels) { channels = 4; }

	pixel previous_pixels[64] = {0};
	pixel_array pixel_buffer = pixel_array_create(scratch, width * height);
	if (!pixel_buffer.raw) {
		LOG_ERR("load_qoi: couldn't create pixel buffer");
		return ret;
	}

	uint64_t write_index = 0; // increment after writing every pixel
	pixel p = {0, 0, 0, 0xff};
	uint8_t run = 0;
	while (write_index < width * height) {
		if (run != 0) {
			run--;
		} else {
			uint8_t byte = _read_u8(&current_offset, raw);

			// try 8 bit operations first:
			if (byte == QOI_OP_RGB) {
				p.r = _read_u8(&current_offset, raw);
				p.g = _read_u8(&current_offset, raw);
				p.b = _read_u8(&current_offset, raw);
				p.a = 0xff; // Set alpha here too in case force_4_channels.
			} else if (byte == QOI_OP_RGBA) {
				p.r = _read_u8(&current_offset, raw);
				p.g = _read_u8(&current_offset, raw);
				p.b = _read_u8(&current_offset, raw);
				p.a = _read_u8(&current_offset, raw);
			} else if ((byte & 0xc0) == QOI_OP_INDEX) {
				p = previous_pixels[byte];
			} else if ((byte & 0xc0) == QOI_OP_DIFF) {
				int8_t b1 = byte;
				p.r += ((b1 >> 4) & 0x03) - 2;
				p.g += ((b1 >> 2) & 0x03) - 2;
				p.b += (b1 & 0x03) - 2;
				p.a = p.a; // alpha doesn't change
			} else if ((byte & 0xc0) == QOI_OP_LUMA) {
				int8_t next_byte = _read_u8(&current_offset, raw);
				int vg = ((int8_t)byte & 0x3f) - 32;

				p.r += vg - 8 + ((next_byte >> 4) & 0x0f);
				p.g += vg;
				p.b += vg - 8 + (next_byte & 0x0f);
				p.a = p.a; // alpha doesn't change
			} else if ((byte & 0xc0) == QOI_OP_RUN) {
				uint8_t run_length = (byte & 0x3f);
				ASSERT(run_length != 63 && run_length != 64);
				run = run_length;
			} else {
				LOG_ERR("BAD BYTE: 0x%x (%u)", byte, byte);
				return ret;
			}
		}

		pixel_array_set(pixel_buffer, write_index++, &p);
		previous_pixels[_get_index(p)] = p;
	}

	LOG_INFO("load_qoi: read %lu pixels", write_index);

	u8_array ret_arr = u8_array_create(arena, width * height * channels);
	if (!ret_arr.raw) {
		LOG_ERR("couldn't create result array");
		return ret;
	}
	uint64_t byte_idx = 0;

	for (uint64_t x = 0; x < width * height; ++x) {
		pixel* idk;
		ASSERT(pixel_array_at(pixel_buffer, &idk, x));
		u8_array_set(ret_arr, byte_idx++, &idk->r);
		u8_array_set(ret_arr, byte_idx++, &idk->g);
		u8_array_set(ret_arr, byte_idx++, &idk->b);
		if (channels == 4) { u8_array_set(ret_arr, byte_idx++, &idk->a); }
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
