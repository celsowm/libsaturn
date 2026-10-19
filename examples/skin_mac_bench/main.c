/* Isolated skinning experiment. Production LibSaturn remains unchanged.
 * Q16 transform and Q15 blend semantics match skin.cpp exactly.
 * SH-1/SH-2 Programming Manual sections 6.29 and 7.5 describe MAC.L's
 * 64-bit mode, SR.S contract, multiplier latency and interrupt interactions.
 */
#include <stdint.h>
#include "saturn/app.h"
#include "saturn/time.h"
#include "saturn/color.h"
#include "saturn/font.h"
#include "saturn/skin3d.h"
#include "example_util.h"

enum {
    kRigidCount = 220,
    kBlendCount = 723,
    kVertexCount = kRigidCount + kBlendCount,
    kJointCount = 61,
    kKernelCount = 2,
    kVectorComponents = 3,
    kMatrixRowStride = 4,
    kMatrixTransformElements = 12,
    kMatrixYDiagonal = 5,
    kMatrixZDiagonal = 10,
    kQ16FractionBits = 16,
    kQ15FractionBits = 15,
    kMatrixIdentityOffset = 131,
    kMatrixPoseOffset = 97,
    kRepetitions = 32,
    kExpectedRows = kKernelCount,
    kMatrixFillLimit = 16385,
    kMatrixFillBias = 8192,
    kTranslationScale = 8,
    kHalfWeightFrequency = 9,
    kRandomMultiplier = 1664525,
    kRandomIncrement = 1013904223,
    kRandomSeed = 0x6d2b79f5,
    kHudX = 8,
    kHudFontWidth = 8,
    kHudTitleY = 8,
    kHudCResultY = 40,
    kHudMacResultY = 64,
    kHudChecksY = 160,
    kHudErrorsY = 184,
    kHudFrameY = 208,
    kPosShift = 15,
    kSampleCount = 5,
    kPoseCount = 4,
    kTelemetryMagic = 0x534b4d31u,
    kTelemetryVersion = 1,
    kWeightOne = 1 << 15,
    kWeightHalf = kWeightOne / 2,
    kQ16One = 1 << 16,
    kRoundQ16 = 1 << 15,
    kRoundQ15 = kWeightOne / 2,
    kSrSaturationBit = 2
};

typedef struct {
    uint32_t kernel, region, repetitions, ticks[kSampleCount], checksum;
} sample_row_t;
typedef struct {
    uint32_t magic, version, frequency, done, errors, checks, ccr, rows;
    sample_row_t samples[kKernelCount];
} telemetry_t;
volatile telemetry_t g_skin_mac_bench;

static sat_skin_rigid_t rigid[kRigidCount];
static sat_skin_blend_t blend[kBlendCount];
static sat_mat4_t joints[kJointCount];
static sat_vec3_t reference_vertices[kVertexCount];
static sat_vec3_t candidate_vertices[kVertexCount];
static sat_skin_asset_t skin = {
    rigid, blend, 0, kRigidCount, kBlendCount, kJointCount, kPosShift, kJointCount - 1
};
static sat_ascii_font_t font;
static uint32_t random_state = kRandomSeed;

static uint32_t next_random(void) {
    random_state = random_state * kRandomMultiplier + kRandomIncrement;
    return random_state;
}
static int16_t random_position(void) {
    return (int16_t)(next_random() >> 16);
}
static uint32_t sr_read(void) {
    uint32_t value;
    __asm__ volatile("stc sr,%0" : "=r"(value));
    return value;
}
static void sr_write(uint32_t value) {
    __asm__ volatile("ldc %0,sr" :: "r"(value) : "memory", "t");
}
static void set_component(sat_vec3_t* output, unsigned axis, int32_t value) {
    if (axis == 0) output->x = value;
    else if (axis == 1) output->y = value;
    else output->z = value;
}

/* A row has three contiguous coefficients followed by its Q16 translation. */
static int64_t dot_mac_l(const int32_t* xyz, const sat_fx16_t* row) {
    uint32_t high, low;
    __asm__ volatile(
        "clrmac\n\t"
        "mac.l @%2+,@%3+\n\t"
        "mac.l @%2+,@%3+\n\t"
        "mac.l @%2+,@%3+\n\t"
        "sts mach,%0\n\t"
        "sts macl,%1"
        : "=&r"(high), "=&r"(low), "+&r"(xyz), "+&r"(row)
        :
        : "mach", "macl", "memory");
    return (int64_t)(int32_t)high * INT64_C(4294967296) + (uint32_t)low;
}

static void transform_mac_l(const sat_fx16_t* matrix, const int16_t* position,
                            int32_t scale, int32_t output[3]) {
    const int32_t xyz[kVectorComponents] = {
        (int32_t)position[0] * scale,
        (int32_t)position[1] * scale,
        (int32_t)position[2] * scale
    };
    for (unsigned axis = 0; axis < kVectorComponents; ++axis) {
        const unsigned base = axis * kMatrixRowStride;
        const int64_t sum = dot_mac_l(xyz, matrix + base) + kRoundQ16;
        output[axis] = (int32_t)(sum >> kQ16FractionBits) + matrix[base + kVectorComponents];
    }
}

static void deform_mac_l(const sat_skin_asset_t* asset, const sat_mat4_t* matrices,
                         sat_vec3_t* output) {
    const int32_t scale = (int32_t)1 << (kQ16FractionBits - asset->pos_shift);
    for (uint32_t vertex = 0; vertex < asset->rigid_count; ++vertex) {
        const sat_skin_rigid_t* record = &asset->rigid[vertex];
        int32_t point[3];
        transform_mac_l(matrices[record->joint].m, record->pos, scale, point);
        output[vertex].x = point[0];
        output[vertex].y = point[1];
        output[vertex].z = point[2];
    }
    for (uint32_t index = 0; index < asset->blend_count; ++index) {
        const sat_skin_blend_t* record = &asset->blend[index];
        const uint32_t vertex = asset->rigid_count + index;
        int32_t point[3][3];
        for (uint8_t influence = 0; influence < record->count; ++influence)
            transform_mac_l(matrices[record->joint[influence]].m,
                            record->pos[influence], scale, point[influence]);
        for (unsigned axis = 0; axis < kVectorComponents; ++axis) {
            if (record->count == 1) {
                set_component(&output[vertex], axis, point[0][axis]);
            } else if (record->count == 2 && record->weight[0] == kWeightHalf) {
                set_component(&output[vertex], axis, (int32_t)(((int64_t)point[0][axis] +
                    point[1][axis]) >> 1));
            } else {
                const int64_t weight0 = record->weight[0];
                const int64_t weight1 = record->count >= 2
                    ? (record->count == 2 ? kWeightOne - weight0 : record->weight[1]) : 0;
                const int64_t weight2 = record->count == 3
                    ? kWeightOne - weight0 - weight1 : 0;
                const int64_t sum = weight0 * point[0][axis] +
                    weight1 * point[1][axis] + weight2 * point[2][axis] + kRoundQ15;
                set_component(&output[vertex], axis, (int32_t)(sum >> kQ15FractionBits));
            }
        }
    }
}

/* Independent quotient/remainder oracle; it does not share the implementation's shifts. */
static int32_t floor_q16(int64_t sum) {
    int64_t quotient = sum / kQ16One;
    if (sum < 0 && sum % kQ16One) --quotient;
    return (int32_t)quotient;
}
static int32_t floor_q15(int64_t sum) {
    int64_t quotient = sum / kWeightOne;
    if (sum < 0 && sum % kWeightOne) --quotient;
    return (int32_t)quotient;
}
static void oracle_transform(const sat_fx16_t* matrix, const int16_t* position,
                             int32_t scale, int32_t output[3]) {
    const int64_t x = (int32_t)position[0] * scale;
    const int64_t y = (int32_t)position[1] * scale;
    const int64_t z = (int32_t)position[2] * scale;
    for (unsigned axis = 0; axis < kVectorComponents; ++axis) {
        const unsigned base = axis * kMatrixRowStride;
        const int64_t sum = (int64_t)matrix[base] * x +
            (int64_t)matrix[base + 1u] * y +
            (int64_t)matrix[base + 2u] * z + kRoundQ16;
        output[axis] = floor_q16(sum) + matrix[base + 3u];
    }
}
static void oracle_deform(const sat_skin_asset_t* asset, const sat_mat4_t* matrices,
                          sat_vec3_t* output) {
    const int32_t scale = (int32_t)1 << (kQ16FractionBits - asset->pos_shift);
    for (uint32_t vertex = 0; vertex < asset->rigid_count; ++vertex) {
        int32_t point[3];
        const sat_skin_rigid_t* record = &asset->rigid[vertex];
        oracle_transform(matrices[record->joint].m, record->pos, scale, point);
        output[vertex].x = point[0]; output[vertex].y = point[1]; output[vertex].z = point[2];
    }
    for (uint32_t index = 0; index < asset->blend_count; ++index) {
        const sat_skin_blend_t* record = &asset->blend[index];
        const uint32_t vertex = asset->rigid_count + index;
        int32_t point[3][3];
        for (uint8_t influence = 0; influence < record->count; ++influence)
            oracle_transform(matrices[record->joint[influence]].m,
                             record->pos[influence], scale, point[influence]);
        for (unsigned axis = 0; axis < kVectorComponents; ++axis) {
            int32_t value;
            if (record->count == 1) value = point[0][axis];
            else if (record->count == 2 && record->weight[0] == kWeightHalf)
                {
                    const int64_t sum = (int64_t)point[0][axis] + point[1][axis];
                    int64_t half = sum / 2;
                    if (sum < 0 && sum % 2) --half;
                    value = (int32_t)half;
                }
            else {
                const int64_t weight0 = record->weight[0];
                const int64_t weight1 = record->count == 2
                    ? kWeightOne - weight0 : record->weight[1];
                const int64_t weight2 = record->count == 3
                    ? kWeightOne - weight0 - weight1 : 0;
                const int64_t sum = weight0 * point[0][axis] +
                    weight1 * point[1][axis] + weight2 * point[2][axis] + kRoundQ15;
                value = floor_q15(sum);
            }
            set_component(&output[vertex], axis, value);
        }
    }
}
static int equal_vertices(const sat_vec3_t* left, const sat_vec3_t* right) {
    for (uint32_t vertex = 0; vertex < kVertexCount; ++vertex)
        if (left[vertex].x != right[vertex].x || left[vertex].y != right[vertex].y ||
            left[vertex].z != right[vertex].z) return 0;
    return 1;
}
static uint32_t checksum_vertices(const sat_vec3_t* vertices) {
    uint32_t hash = 0;
    for (uint32_t vertex = 0; vertex < kVertexCount; ++vertex) {
        hash = hash * 33u + (uint32_t)vertices[vertex].x;
        hash = hash * 33u + (uint32_t)vertices[vertex].y;
        hash = hash * 33u + (uint32_t)vertices[vertex].z;
    }
    return hash;
}
static void fill_inputs(void) {
    for (uint16_t joint = 0; joint < kJointCount; ++joint) {
        sat_fx16_t* matrix = joints[joint].m;
        for (unsigned i = 0; i < kMatrixTransformElements; ++i) {
            const int32_t centered = (int32_t)(next_random() % kMatrixFillLimit) - kMatrixFillBias;
            matrix[i] = (sat_fx16_t)centered;
        }
        matrix[0] += kQ16One;
        matrix[kMatrixYDiagonal] += kQ16One;
        matrix[kMatrixZDiagonal] += kQ16One;
        matrix[kVectorComponents] *= kTranslationScale;
        matrix[kMatrixRowStride + kVectorComponents] *= kTranslationScale;
        matrix[2u * kMatrixRowStride + kVectorComponents] *= kTranslationScale;
    }
    for (uint32_t index = 0; index < kRigidCount; ++index) {
        rigid[index].pos[0] = random_position();
        rigid[index].pos[1] = random_position();
        rigid[index].pos[2] = random_position();
        rigid[index].joint = (uint8_t)(next_random() % kJointCount);
        rigid[index].reserved = 0;
    }
    rigid[0].joint = kJointCount - 1u;
    for (uint32_t index = 0; index < kBlendCount; ++index) {
        sat_skin_blend_t* record = &blend[index];
        record->count = (uint8_t)(1u + index % 3u);
        for (uint8_t influence = 0; influence < 3; ++influence) {
            record->joint[influence] = (uint8_t)(next_random() % kJointCount);
            for (unsigned axis = 0; axis < 3; ++axis)
                record->pos[influence][axis] = random_position();
        }
        record->weight[0] = index % kHalfWeightFrequency == 0u
            ? kWeightHalf : (uint16_t)(next_random() % kWeightOne);
        record->weight[1] = (uint16_t)(next_random() % (kWeightOne - record->weight[0] + 1u));
    }
}
static void qualify(void) {
    const uint32_t saved = sr_read();
    for (unsigned pose = 0; pose < kPoseCount; ++pose) {
        for (uint16_t joint = 0; joint < kJointCount; ++joint) {
            sat_fx16_t* matrix = joints[joint].m;
            matrix[kVectorComponents] += (sat_fx16_t)(pose * kMatrixIdentityOffset);
            matrix[kMatrixRowStride + kVectorComponents] -= (sat_fx16_t)(pose * kMatrixPoseOffset);
        }
        if (sat_skin_deform_range(&skin, joints, kJointCount, 0, kVertexCount,
                                  reference_vertices, kVertexCount) != SAT_OK)
            ++g_skin_mac_bench.errors;
        sr_write(saved & ~kSrSaturationBit);
        deform_mac_l(&skin, joints, candidate_vertices);
        sr_write(saved);
        oracle_deform(&skin, joints, reference_vertices);
        if (!equal_vertices(reference_vertices, candidate_vertices)) ++g_skin_mac_bench.errors;
        sr_write(saved & ~kSrSaturationBit);
        deform_mac_l(&skin, joints, candidate_vertices);
        sr_write(saved);
        if (!equal_vertices(reference_vertices, candidate_vertices)) ++g_skin_mac_bench.errors;
        g_skin_mac_bench.checks += kVertexCount * 2u;
    }
}
static uint32_t median(const volatile uint32_t* values) {
    uint32_t sorted[kSampleCount];
    for (unsigned index = 0; index < kSampleCount; ++index) {
        uint32_t value = values[index]; unsigned position = index;
        while (position && sorted[position - 1u] > value) {
            sorted[position] = sorted[position - 1u]; --position;
        }
        sorted[position] = value;
    }
    return sorted[kSampleCount / 2u];
}
static void measure(void) {
    for (unsigned kernel = 0; kernel < kKernelCount; ++kernel) {
        volatile sample_row_t* row = &g_skin_mac_bench.samples[kernel];
        row->kernel = kernel; row->region = 0; row->repetitions = kRepetitions;
        for (unsigned sample = 0; sample < kSampleCount; ++sample) {
            uint32_t hash = 0;
            const uint32_t start = (uint32_t)sat_time_ticks();
            for (uint32_t repetition = 0; repetition < row->repetitions; ++repetition) {
                if (kernel == 0) {
                    if (sat_skin_deform_range(&skin, joints, kJointCount, 0, kVertexCount,
                                              reference_vertices, kVertexCount) != SAT_OK)
                        ++g_skin_mac_bench.errors;
                } else {
                    const uint32_t saved = sr_read();
                    sr_write(saved & ~kSrSaturationBit);
                    deform_mac_l(&skin, joints, candidate_vertices);
                    sr_write(saved);
                }
                hash = hash * 33u + checksum_vertices(kernel ? candidate_vertices : reference_vertices);
            }
            row->ticks[sample] = (uint32_t)sat_time_ticks() - start;
            row->checksum = hash;
            if (kernel == 1 && !equal_vertices(reference_vertices, candidate_vertices))
                ++g_skin_mac_bench.errors;
        }
    }
    if (g_skin_mac_bench.samples[0].checksum != g_skin_mac_bench.samples[1].checksum)
        ++g_skin_mac_bench.errors;
}
int main(void) {
    sat_example_must(sat_app_init_default());
    g_skin_mac_bench.magic = kTelemetryMagic;
    g_skin_mac_bench.version = kTelemetryVersion;
    g_skin_mac_bench.frequency = sat_time_frequency_hz();
    g_skin_mac_bench.errors = g_skin_mac_bench.checks = 0;
    g_skin_mac_bench.ccr = *(volatile uint8_t*)0xfffffe92u;
    g_skin_mac_bench.rows = kExpectedRows;
    fill_inputs();
    if (sat_skin_validate(&skin) != SAT_OK) ++g_skin_mac_bench.errors;
    qualify();
    measure();
    g_skin_mac_bench.done = 1;
    sat_example_must(sat_ascii_font_init_8x8_indexed8(&font, SAT_COLOR_WHITE, 0, 0));
    uint32_t frame = 0;
    for (;;) {
        sat_example_must(sat_app_frame_begin(SAT_COLOR_BLACK, SAT_COLOR_BLACK, 0));
        sat_example_must(sat_ascii_font_draw_text_screen_indexed8(&font,
            "EXACT SKIN C / MAC.L", kHudX, kHudTitleY, kHudFontWidth, 0, 0));
        sat_example_must(sat_ascii_font_draw_label_u32(&font, "C API TICKS ",
            median(g_skin_mac_bench.samples[0].ticks), kHudX, kHudCResultY, kHudFontWidth, 0, 0));
        sat_example_must(sat_ascii_font_draw_label_u32(&font, "MAC.L TICKS ",
            median(g_skin_mac_bench.samples[1].ticks), kHudX, kHudMacResultY, kHudFontWidth, 0, 0));
        sat_example_must(sat_ascii_font_draw_label_u32(&font, "CHECKS ",
            g_skin_mac_bench.checks, kHudX, kHudChecksY, kHudFontWidth, 0, 0));
        sat_example_must(sat_ascii_font_draw_label_u32(&font, "ERRORS ",
            g_skin_mac_bench.errors, kHudX, kHudErrorsY, kHudFontWidth, 0, 0));
        sat_example_must(sat_ascii_font_draw_label_u32(&font, "FRAME ", ++frame,
            kHudX, kHudFrameY, kHudFontWidth, 0, 0));
        sat_example_must(sat_app_frame_end());
    }
}
