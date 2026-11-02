/* The codec table of a program that carries only ADX from the disc (saturn/music_codecs.h): the library's table
 * (src/audio/music/codecs.cpp, with the PCM and IMA delivery, the RAM and asset feeds and the IMA tables) is not
 * linked, the ADX track plays exactly as in test_music_disc, and every other format is SAT_ERR_UNSUPPORTED. The
 * scenarios are test_music_disc.cpp's. */
#define SAT_TEST_LEAN_CODECS 1
#include "test_music_disc.cpp"
