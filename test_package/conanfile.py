import os

from conans import ConanFile, CMake, tools
from conans.errors import ConanException


class TestConan(ConanFile):
    settings = "os", "arch", "compiler", "build_type"
    generators = "cmake", "cmake_find_package_multi"

    requires = [
    ]
    build_requires = [
        "upb/[]",
        "cmake/[]"
    ]

    def build(self):
        cmake = CMake(self)
        defs = {}
        if tools.cross_building(self):
            defs["CMAKE_TRY_COMPILE_TARGET_TYPE"] = "STATIC_LIBRARY"
        cmake.configure(defs=defs)
        cmake.configure()
        # cmake.configure(source_folder="conan")
        cmake.build()

    def test(self):
        if not tools.cross_building(self, skip_x64_x86=True) or (self.settings.os == "Macos" and self.settings.arch == 'armv8'):
            os.chdir("bin")
            for exe in os.scandir(os.getcwd()):
                try:
                    self.run(exe.path)
                    self.output.success("Passed: {}".format(exe.path))
                except:
                    self.output.error("Failed: {}".format(exe.path))
                    raise
            self.output.success("All tests built and executed successfully!")
        else:
            self.output.success(
                "Conan detected a cross-build, no tests executed")

    def imports(self):
        self.copy("*.dll", "", "bin")
        self.copy("*.dylib", "", "bin")
        self.copy("*.so*", "", "bin")
