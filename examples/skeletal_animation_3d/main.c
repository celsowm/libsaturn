#if defined(SKELETAL_ROBOT_DEMO) && SKELETAL_ROBOT_DEMO
#include "robot_demo.inc"
#else
#include "examples/characters/demo.h"
int main(void) { return character_demo_run(CHARACTER_DEMO_VIEWER); }
#endif
