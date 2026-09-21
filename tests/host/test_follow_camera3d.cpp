#include <cstdio>
#include <cstdlib>

#include "saturn/follow_camera3d.h"

#define TEST(name) static void name()
#define ASSERT_EQ(a,b) do { if ((a)!=(b)) { std::fprintf(stderr,"FAIL %s:%d\n",__FILE__,__LINE__); std::exit(1); } } while (0)
#define ASSERT_TRUE(x) ASSERT_EQ((x),true)
#define FX(v) ((sat_fx16_t)((v)*65536))

TEST(rejects_invalid_policy) {
    sat_follow_camera3d_t c={};
    const sat_vec3_t origin={0,0,0};
    ASSERT_EQ(sat_follow_camera3d_init(nullptr,&origin,1u,1u,1u),SAT_ERR_INVALID_ARG);
    ASSERT_EQ(sat_follow_camera3d_init(&c,&origin,0u,1u,1u),SAT_ERR_INVALID_ARG);
    ASSERT_EQ(sat_follow_camera3d_init(&c,&origin,4u,6u,4u),SAT_OK);
    ASSERT_EQ(sat_follow_camera3d_set_offsets(&c,nullptr,&origin),SAT_ERR_INVALID_ARG);
}

TEST(smooths_and_snaps_with_offsets) {
    sat_follow_camera3d_t c={};
    const sat_vec3_t origin={0,0,0};
    const sat_vec3_t desired={FX(8),FX(6),FX(-4)};
    const sat_vec3_t eye_offset={FX(1),FX(29),FX(2)};
    const sat_vec3_t target_offset={0,FX(5),FX(3)};
    sat_vec3_t eye={},target={};
    ASSERT_EQ(sat_follow_camera3d_init(&c,&origin,4u,6u,4u),SAT_OK);
    ASSERT_EQ(sat_follow_camera3d_set_offsets(&c,&eye_offset,&target_offset),SAT_OK);
    ASSERT_EQ(sat_follow_camera3d_step(&c,&desired,0u,&eye,&target),SAT_OK);
    ASSERT_EQ(c.anchor.x,FX(2));
    ASSERT_EQ(c.anchor.y,FX(1));
    ASSERT_EQ(c.anchor.z,FX(-1));
    ASSERT_EQ(eye.x,FX(3));
    ASSERT_EQ(eye.y,FX(30));
    ASSERT_EQ(target.z,FX(2));
    ASSERT_EQ(sat_follow_camera3d_step(&c,&desired,1u,&eye,&target),SAT_OK);
    ASSERT_EQ(c.anchor.x,desired.x);
    ASSERT_EQ(eye.y,FX(35));
    ASSERT_TRUE(target.x==desired.x);
}

int main() {
    rejects_invalid_policy();
    smooths_and_snaps_with_offsets();
    std::puts("test_follow_camera3d: 2 tests passed");
    return 0;
}
