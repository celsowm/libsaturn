#ifndef SATURN_VERSION_H
#define SATURN_VERSION_H

/* GENERATED from the repository VERSION file by cmake/LibSaturnVersion.cmake.
 * Do not edit by hand: change VERSION, then run
 *   cmake -P cmake/SyncVersion.cmake
 * The CMake configure step fails if this header disagrees with VERSION. */

#define SATURN_VERSION_MAJOR 0
#define SATURN_VERSION_MINOR 1
#define SATURN_VERSION_PATCH 0
#define SATURN_VERSION_STRING "0.1.0"

/* Monotonic integer for preprocessor comparisons: MMmmpp, e.g. 0.1.0 is
 * 100. Minor and patch each take two decimal digits. */
#define SATURN_VERSION_NUMBER \
    ((SATURN_VERSION_MAJOR * 10000) + (SATURN_VERSION_MINOR * 100) + \
     SATURN_VERSION_PATCH)

#define SATURN_VERSION_AT_LEAST(major, minor, patch) \
    (SATURN_VERSION_NUMBER >= (((major) * 10000) + ((minor) * 100) + (patch)))

#endif /* SATURN_VERSION_H */
