from conan import ConanFile
from conan.tools.cmake import CMake, CMakeToolchain, cmake_layout
from conan.tools.files import copy, get
from conan.tools.scm import Version


class FmtConan(ConanFile):
    name = "fmt"
    license = "MIT"
    url = "https://github.com/conan-io/conan-center-index"
    homepage = "https://fmt.dev"
    description = "A modern formatting library"
    topics = ("format", "iostream", "printf")
    settings = "os", "arch", "compiler", "build_type"
    options = {
        "shared": [True, False],
        "fPIC": [True, False],
        "header_only": [True, False],
        "with_fmt_alias": [True, False],
        "with_os_api": [True, False],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
        "header_only": False,
        "with_fmt_alias": False,
        "with_os_api": True,
    }
    package_type = "library"

    def config_options(self):
        if self.settings.os == "Windows":
            del self.options.fPIC

    def configure(self):
        if self.options.header_only:
            self.options.rm_safe("fPIC")
            self.options.rm_safe("shared")
        elif self.options.shared:
            self.options.rm_safe("fPIC")

    def layout(self):
        cmake_layout(self, src_folder="src")

    def source(self):
        get(self, **self.conan_data["sources"][self.version], strip_root=True)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.variables["FMT_DOC"] = False
        tc.variables["FMT_TEST"] = False
        tc.variables["FMT_INSTALL"] = True
        tc.variables["FMT_LIB_DIR"] = "lib"
        tc.generate()

    def build(self):
        if not self.options.header_only:
            cmake = CMake(self)
            cmake.configure()
            cmake.build()

    def package(self):
        copy(self, "LICENSE*", src=self.source_folder, dst=self.package_folder)
        if self.options.header_only:
            copy(self, "*.h", src=self.source_folder, dst=self.package_folder)
        else:
            cmake = CMake(self)
            cmake.install()

    def package_info(self):
        self.cpp_info.set_property("cmake_find_mode", "both")
        self.cpp_info.set_property("cmake_file_name", "fmt")
        self.cpp_info.set_property("cmake_target_name", "fmt::fmt")
        self.cpp_info.set_property("pkg_config_name", "fmt")
        if self.options.header_only:
            self.cpp_info.bindirs = []
            self.cpp_info.libdirs = []
        else:
            postfix = "d" if self.settings.build_type == "Debug" else ""
            self.cpp_info.libs = ["fmt" + postfix]
        if Version(self.version) >= "9.0.0" and self.settings.os in ("Linux", "FreeBSD"):
            self.cpp_info.system_libs.append("m")
