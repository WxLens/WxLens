from conan import ConanFile
from conan.tools.cmake import CMake
from conan.tools.files import copy
import os

class NimbusConan(ConanFile):
    # Dependencies needed to link wxdata (external/legacy-supercell-wx/wxdata) and its GTest
    # suite unmodified, per docs/ROADMAP.md §3.1/§7 Phase 0. MapLibre Native Qt (the map renderer)
    # is built from its vendored submodule directly, not through Conan - see
    # external/maplibre-native-qt.cmake and docs/adr/0004-maplibre-qml-integration.md.
    settings   = ("os", "compiler", "build_type", "arch")
    requires   = ("boost/1.91.0",
                  "bzip2/1.0.8",
                  "cpr/1.14.2",
                  "glm/1.0.1",
                  "gtest/1.17.0",
                  "libxml2/2.15.3",
                  "libzip/1.11.4",
                  "openssl/3.6.3",
                  "range-v3/cci.20240905",
                  "re2/20251105",
                  "spdlog/1.17.0",
                  "zlib/1.3.2")
    generators = ("CMakeDeps")
    default_options = {"boost/*:without_cobalt": True}

    def configure(self):
        if self.settings.os == "Windows":
            self.options["libcurl"].with_ssl = "schannel"
        elif self.settings.os == "Linux":
            self.options["openssl"].shared    = True
            self.options["libcurl"].ca_bundle = "none"
            self.options["libcurl"].ca_path   = "none"
        elif self.settings.os == "Macos":
            self.options["openssl"].shared    = True
            self.options["libcurl"].ca_bundle = "none"
            self.options["libcurl"].ca_path   = "none"

    def requirements(self):
        if self.settings.os == "Linux":
            self.requires("opengl/system")
            self.requires("onetbb/2023.0.0")

    def generate(self):
        build_folder = os.path.join(self.build_folder,
                                    "..",
                                    str(self.settings.build_type),
                                    self.cpp_info.bindirs[0])

        for dep in self.dependencies.values():
            if dep.cpp_info.bindirs:
                copy(self, "*.dll", dep.cpp_info.bindirs[0], build_folder)
            if dep.cpp_info.libdirs:
                copy(self, "*.dylib", dep.cpp_info.libdirs[0], build_folder)

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        cmake = CMake(self)
        cmake.install()
