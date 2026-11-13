"""C++11 host assertions for bounded forwarding and mute/notification policy."""
import os
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path


class ForwardingPolicyHostTests(unittest.TestCase):
    def test_forwarding_and_notice_rate_with_release_assertions(self):
        compiler = shutil.which("g++") or shutil.which("clang++")
        # A working MSVC Build Tools installation is common on Windows even
        # when neither GCC nor Clang is on PATH. Prefer native CMake/MSVC over
        # launching WSL solely to compile one platform-independent test.
        vs_root = Path(os.environ.get("ProgramFiles(x86)", r"C:\Program Files (x86)")) / (
            "Microsoft Visual Studio/2022/BuildTools/VC/Tools/MSVC")
        has_msvc = os.name == "nt" and vs_root.is_dir() and any(
            vs_root.glob("*/bin/Hostx64/x64/cl.exe"))
        use_cmake_msvc = compiler is None and has_msvc and shutil.which("cmake") is not None
        use_wsl = compiler is None and not use_cmake_msvc and os.name == "nt" and shutil.which("wsl.exe") is not None
        if compiler is None and not use_cmake_msvc and not use_wsl:
            self.skipTest("No usable C++11 host compiler (GCC/Clang or CMake/MSVC)")
        with tempfile.TemporaryDirectory() as directory:
            source = str(Path(__file__).resolve().parent / "ha_forward_policy.cpp")
            executable = str(Path(directory) / ("ha_forward_policy.exe" if os.name == "nt" else "ha_forward_policy"))
            flags = ["-std=c++11", "-Wall", "-Wextra", "-Werror", "-O2", "-DNDEBUG"]
            if use_cmake_msvc:
                cmake_source = Path(directory) / "CMakeLists.txt"
                cmake_source.write_text(
                    "cmake_minimum_required(VERSION 3.20)\n"
                    "project(ha_forward_policy LANGUAGES CXX)\n"
                    "add_executable(ha_forward_policy \"" +
                    Path(source).as_posix() + "\")\n"
                    "set_property(TARGET ha_forward_policy PROPERTY CXX_STANDARD 11)\n"
                    "target_compile_definitions(ha_forward_policy PRIVATE _CRT_SECURE_NO_WARNINGS)\n"
                    "target_compile_options(ha_forward_policy PRIVATE /W4 /WX /O2 /DNDEBUG)\n",
                    encoding="utf-8")
                config = ["cmake", "-S", directory, "-B", str(Path(directory) / "build"),
                          "-G", "Visual Studio 17 2022", "-A", "x64"]
                for command, deadline in ((config, 45),
                                          (["cmake", "--build", str(Path(directory) / "build"),
                                            "--config", "Release"], 90)):
                    result = subprocess.run(command, capture_output=True, text=True,
                                            timeout=deadline)
                    self.assertEqual(result.returncode, 0,
                                     f"Native MSVC gate failed: {result.stdout}\n{result.stderr}")
                run_cmd = [str(Path(directory) / "build/Release/ha_forward_policy.exe")]
            elif use_wsl:
                def linux_path(path):
                    try:
                        return subprocess.run(["wsl.exe", "--exec", "wslpath", "-a", path],
                                              check=True, capture_output=True, text=True,
                                              timeout=20).stdout.strip()
                    except subprocess.TimeoutExpired as error:
                        self.fail(f"WSL path conversion hung ({error.timeout}s); "
                                  "host C++ gate must not silently skip on timeout")

                linux_exe = linux_path(executable)
                compile_cmd = ["wsl.exe", "--exec", "g++", *flags, linux_path(source), "-o", linux_exe]
                run_cmd = ["wsl.exe", "--exec", linux_exe]
            else:
                compile_cmd = [compiler, *flags, source, "-o", executable]
                run_cmd = [executable]
            if not use_cmake_msvc:
                subprocess.run(compile_cmd, check=True, capture_output=True, text=True, timeout=45)
            subprocess.run(run_cmd, check=True, capture_output=True, text=True, timeout=20)


if __name__ == "__main__":
    unittest.main()
