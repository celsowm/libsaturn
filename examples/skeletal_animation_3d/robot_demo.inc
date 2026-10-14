/* Robot Police Unit, CC BY 4.0. See assets/ROBOT_POLICE_ATTRIBUTION.md. */
#include "robot_police_skeletal.h"
#define SKELETAL_VIEWER_TITLE "ROBOT POLICE / SKELETAL DEMO"
#define VIEWER_ASSET robot_police_skeletal_asset
#define VIEWER_SKIN robot_police_skeletal_skin
#define VIEWER_SKELETON robot_police_skeletal_skeleton
#define VIEWER_VERTEX_COUNT ROBOT_POLICE_SKELETAL_VERTEX_COUNT
#define VIEWER_FACE_COUNT ROBOT_POLICE_SKELETAL_FACE_COUNT
#define VIEWER_TEXTURE_COUNT ROBOT_POLICE_SKELETAL_TEXTURE_COUNT
#define VIEWER_BIND_MIN robot_police_skeletal_animated_min
#define VIEWER_BIND_MAX robot_police_skeletal_animated_max
#define VIEWER_JOINT_CAPACITY 128u
#define VIEWER_CLIPS &robot_police_skeletal_clip__3903460652032_TempMotion
#define VIEWER_CLIP_NAMES "GESTURE"
#include "examples/common/skeletal_viewer.inc"
