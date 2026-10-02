"""Conan test_package: build the standalone package consumer through Conan.

It reuses tests/package_consumer, the same project the plain-CMake gate links
against `cmake --install`'s output, and the same contract checks
(tests/package_consumer/check.cmake). Conan only changes how the prefix reaches
the consumer: CMakeToolchain puts the libsaturn package folder on
CMAKE_PREFIX_PATH and the consumer's unmodified
`find_package(LibSaturn CONFIG REQUIRED)` resolves the installed config.
"""
import os
from io import StringIO

from conan import ConanFile
from conan.tools.cmake import CMake, CMakeToolchain, cmake_layout


class LibSaturnTestPackage(ConanFile):
    settings = "os", "arch", "compiler", "build_type"
    generators = "CMakeDeps"
    test_type = "explicit"

    def requirements(self):
        self.requires(self.tested_reference_str)

    def layout(self):
        cmake_layout(self)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.cache_variables["CMAKE_EXPORT_COMPILE_COMMANDS"] = True
        tc.cache_variables["LIBSATURN_REQUIRED_VERSION"] = str(
            self.dependencies["libsaturn"].ref.version
        )
        tc.generate()

    def _consumer_source(self):
        return os.path.join(self.recipe_folder, os.pardir, "tests", "package_consumer")

    def build(self):
        cmake = CMake(self)
        cmake.configure(build_script_folder=self._consumer_source())
        cmake.build()
        cmake.build(target="consumer_disc")

    def test(self):
        # The image is for the Saturn, never runnable here; verify the link
        # instead. A fresh verbose link exposes the real link line.
        log = StringIO()
        self.run(
            f'cmake --build "{self.build_folder}" --clean-first --verbose',
            stdout=log,
        )
        log_file = os.path.join(self.build_folder, "consumer-build.log")
        with open(log_file, "w", encoding="utf-8") as handle:
            handle.write(log.getvalue())

        # --clean-first removed the disc too; libsaturn_add_disc() rebuilds it.
        self.run(f'cmake --build "{self.build_folder}" --target consumer_disc')

        prefix = self.dependencies["libsaturn"].package_folder
        source_dir = os.path.abspath(os.path.join(self.recipe_folder, os.pardir))
        self.run(
            f'cmake "-DSOURCE_DIR={source_dir}" "-DPREFIX={prefix}" '
            f'"-DCONSUMER_DIR={self.build_folder}" "-DBUILD_LOG={log_file}" '
            f'-P "{os.path.join(self._consumer_source(), "check.cmake")}"'
        )
