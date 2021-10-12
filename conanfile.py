#!/usr/bin/env python
# -*- coding: utf-8 -*-

# Note: Conan is supported on a best-effort basis. upb doesn't use Conan
# internally, so we won't know if it stops working. We may ask community
# members to help us debug any problems that arise.

import os
from conans import ConanFile, CMake, tools
from conans.errors import ConanInvalidConfiguration
from conans.model.version import Version


class upbConan(ConanFile):
    name = "upb"
    version = "0.0.1"
    url = "https://github.com/protocolbuffers/upb"
    homepage = url
    author = "Max Christy <tchristy001@outlook.com>"
    description = "A small protobuf implementation written in C"
    license = "Google LLC"
    topics = ("conan", "upb", "protoc-gen-upb", "protobuf", "google")
    exports = ["LICENSE"]
    exports_sources = ["cmake/*", "upb/*", "upbc/*"]
    generators = ["virtualenv", "cmake", "cmake_find_package"]
    settings = "os", "arch", "compiler", "build_type"

    def requirements(self):
        if not tools.cross_building(self):
            self.requires("abseil/[]")
            self.requires("protobuf/[]")

    def _configure_cmake(self):
        cmake = CMake(self)
        self.enable_testing = any(i in ["Catch2", "cmocka"]
                                  for i in self.deps_cpp_info.deps)
        defs = {
            "BUILD_TESTING": self.enable_testing,
            "CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS": True
        }
        if tools.cross_building(self):
          defs["CMAKE_TRY_COMPILE_TARGET_TYPE"] = "STATIC_LIBRARY"
        cmake.configure(
            defs=defs,
            build_folder=self.build_folder,
            source_folder=os.path.join(self.source_folder, "cmake")
        )
        return cmake

    def configure(self):
        if self.settings.os == "Windows" and \
           self.settings.compiler == "Visual Studio" and \
           Version(self.settings.compiler.version.value) < "14":
            raise ConanInvalidConfiguration("upb does not support MSVC < 14")
        if not tools.valid_min_cppstd(self, "17"):
            raise ConanInvalidConfiguration("C++17 or greater required")

    def build(self):
        cmake = self._configure_cmake()
        cmake.build()
        if self.enable_testing:
            try:
                cmake.test()
            except ConanException:
                print("One or more unit tests failed")
            else:
                print("All unit tests successful")

    def test(self):
        cmake = CMake(self)
        try:
            cmake.test()
        except ConanException:
            print("One or more unit tests failed")
        else:
            print("All unit tests successful")

    def package(self):
        cmake = self._configure_cmake()
        cmake.install()
        self.copy("LICENSE", dst="licenses")

    def package_info(self):
        # the Windows build uses a different destination folder for some reason
        if "Windows" != self.settings.os:
            self.cpp_info.build_modules.append(
                os.path.join("lib", "cmake", "Modules", "ProtocGenUpb.cmake"))
        else:
            self.cpp_info.build_modules.append(
                os.path.join("lib", "cmake", "Modules", "ProtocGenUpb.cmake"))
        self.env_info.PATH.append(os.path.join(self.package_folder, "bin"))

        self.cpp_info.libs = tools.collect_libs(self)

