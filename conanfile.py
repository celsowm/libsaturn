"""Conan 2 recipe for LibSaturn.

Conan is only a delivery mechanism. The recipe configures, builds and
installs the repository's CMake project and republishes the installed CMake
package unchanged, so a Conan consumer sees exactly what `cmake --install`
produces: the same headers, the same `LibSaturn::Saturn` target, the same
`libsaturn_configure_executable()` helper and the same linker script. It does
not restate compiler flags or source lists; those belong to CMakeLists.txt and
cmake/LibSaturnSources.cmake.

The version is read from the VERSION file, the single authoritative project
version (see cmake/LibSaturnVersion.cmake).

Saturn settings (`os=baremetal`, `arch=sh2eb`) come from
packaging/conan/config; install them first:

    conan config install packaging/conan/config
    conan create . --profile:host=saturn-sh2eb --profile:build=default
"""
import os

from conan import ConanFile
from conan.errors import ConanInvalidConfiguration
from conan.tools.cmake import CMake, CMakeToolchain, cmake_layout
from conan.tools.files import copy, load


class LibSaturnConan(ConanFile):
    name = "libsaturn"
    description = "Bare-metal game-development library for the Sega Saturn"
    license = "MIT"
    url = "https://github.com/celsowm/libsaturn"
    homepage = "https://github.com/celsowm/libsaturn"
    topics = ("sega-saturn", "sh2", "bare-metal", "game-development")

    package_type = "static-library"
    settings = "os", "arch", "compiler", "build_type"
    options = {
        "profile_metrics": [True, False],
        "parallel_test_fault": ["ANY"],
    }
    default_options = {
        "profile_metrics": False,
        "parallel_test_fault": "0",
    }

    exports_sources = (
        "CMakeLists.txt",
        "VERSION",
        "LICENSE",
        "cmake/*",
        "include/*",
        "src/*",
    )

    def set_version(self):
        self.version = load(self, os.path.join(self.recipe_folder, "VERSION")).strip()

    def validate(self):
        if str(self.settings.os) != "baremetal" or str(self.settings.arch) != "sh2eb":
            raise ConanInvalidConfiguration(
                "libsaturn only builds for the Sega Saturn: use "
                "--profile:host=saturn-sh2eb (os=baremetal, arch=sh2eb). "
                "Install the settings with `conan config install "
                "packaging/conan/config`."
            )

    def layout(self):
        cmake_layout(self)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.cache_variables["LIBSATURN_BUILD_TESTS"] = False
        tc.cache_variables["LIBSATURN_BUILD_EXAMPLES"] = False
        tc.cache_variables["LIBSATURN_BUILD_HOST_TOOLS"] = False
        tc.cache_variables["LIBSATURN_INSTALL"] = True
        tc.cache_variables["LIBSATURN_ENABLE_PROFILE_METRICS"] = bool(
            self.options.profile_metrics
        )
        tc.cache_variables["LIBSATURN_PARALLEL_TEST_FAULT"] = str(
            self.options.parallel_test_fault
        )
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        copy(self, "LICENSE", self.source_folder,
             os.path.join(self.package_folder, "licenses"))
        CMake(self).install()

    def package_info(self):
        # The installed LibSaturnConfig.cmake is the package. Conan must not
        # generate a second, competing description: hand the prefix to CMake's
        # find_package(LibSaturn CONFIG) and let the real config answer.
        self.cpp_info.set_property("cmake_find_mode", "none")
        self.cpp_info.set_property("cmake_file_name", "LibSaturn")
        self.cpp_info.builddirs = [os.path.join("lib", "cmake", "LibSaturn")]
        self.cpp_info.libs = []
        self.cpp_info.libdirs = []
        self.cpp_info.includedirs = []
        self.cpp_info.bindirs = []
