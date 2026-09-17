#include "saturn/surface.h"
#include "src/core/surface_logic.hpp"

extern "C" uint8_t sat_pixel_format_bytes_per_pixel(sat_pixel_format_t format) {
    return saturn::core::surface_logic::bytes_per_pixel(format);
}

extern "C" sat_result_t sat_surface_init(
    sat_surface_t* surface, void* pixels, uint16_t width, uint16_t height, uint16_t pitch,
    sat_pixel_format_t format, const uint16_t* palette_rgb555, uint16_t palette_count) {
    return saturn::core::surface_logic::init(surface, pixels, width, height, pitch, format, palette_rgb555, palette_count);
}

extern "C" sat_result_t sat_surface_subview(const sat_surface_t* source, const sat_rect_t* rect, sat_surface_t* out_surface) {
    return saturn::core::surface_logic::subview(source, rect, out_surface);
}

extern "C" sat_result_t sat_surface_fill(sat_surface_t* surface, const sat_rect_t* rect, sat_color_t color) {
    return saturn::core::surface_logic::fill(surface, rect, color);
}

extern "C" sat_result_t sat_surface_blit(
    sat_surface_t* destination, sat_point_t destination_position, const sat_surface_t* source, const sat_rect_t* src_rect) {
    return saturn::core::surface_logic::blit(destination, destination_position, source, src_rect);
}

extern "C" sat_result_t sat_surface_blit_scaled(
    sat_surface_t* destination, const sat_rect_t* destination_rect, const sat_surface_t* source, const sat_rect_t* src_rect) {
    return saturn::core::surface_logic::blit_scaled(destination, destination_rect, source, src_rect);
}

extern "C" sat_result_t sat_surface_convert(sat_surface_t* destination, const sat_surface_t* source) {
    return saturn::core::surface_logic::convert(destination, source);
}

extern "C" sat_result_t sat_surface_get_pixel(
    const sat_surface_t* surface, uint16_t x, uint16_t y, sat_color_t* out_color) {
    return saturn::core::surface_logic::get_pixel(surface, x, y, out_color);
}

extern "C" sat_result_t sat_surface_set_pixel(sat_surface_t* surface, uint16_t x, uint16_t y, sat_color_t color) {
    return saturn::core::surface_logic::set_pixel(surface, x, y, color);
}
