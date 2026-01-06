"""
BGP Simulator Python Package Setup

This setup.py enables installation of the BGP simulator Python bindings.
"""

import os
import sys
import subprocess
from pathlib import Path

from setuptools import setup, Extension
from setuptools.command.build_ext import build_ext


class CMakeExtension(Extension):
    def __init__(self, name, sourcedir=''):
        Extension.__init__(self, name, sources=[])
        self.sourcedir = os.path.abspath(sourcedir)


class CMakeBuild(build_ext):
    def build_extension(self, ext):
        extdir = os.path.abspath(os.path.dirname(self.get_ext_fullpath(ext.name)))

        # Required for auto-detection of auxiliary "native" libs
        if not extdir.endswith(os.path.sep):
            extdir += os.path.sep

        cmake_args = [
            f'-DCMAKE_LIBRARY_OUTPUT_DIRECTORY={extdir}',
            f'-DPYTHON_EXECUTABLE={sys.executable}',
            '-DCMAKE_BUILD_TYPE=Release',
        ]

        build_args = ['--config', 'Release']

        # Detect number of CPUs for parallel build
        if hasattr(os, 'cpu_count'):
            build_args += ['--', f'-j{os.cpu_count()}']
        else:
            build_args += ['--', '-j2']

        env = os.environ.copy()
        env['CXXFLAGS'] = f"{env.get('CXXFLAGS', '')} -DVERSION_INFO=\\'{self.distribution.get_version()}\\'"

        if not os.path.exists(self.build_temp):
            os.makedirs(self.build_temp)

        # Run CMake configuration
        print(f"Running CMake configuration...")
        subprocess.check_call(['cmake', ext.sourcedir] + cmake_args, cwd=self.build_temp, env=env)

        # Run CMake build
        print(f"Building with CMake...")
        subprocess.check_call(['cmake', '--build', '.'] + build_args, cwd=self.build_temp)


def read_file(filename):
    """Read file contents"""
    filepath = Path(__file__).parent / filename
    if filepath.exists():
        return filepath.read_text()
    return ""


setup(
    name="bgp-simulator",
    version="1.0.0",
    author="BGP Simulator Team",
    description="High-performance BGP route propagation simulator with ROV support",
    long_description=read_file("README.md"),
    long_description_content_type="text/markdown",
    ext_modules=[CMakeExtension("bgp_simulator")],
    cmdclass={"build_ext": CMakeBuild},
    zip_safe=False,
    python_requires=">=3.6",
    setup_requires=[
        "pybind11>=2.6.0",
    ],
    install_requires=[
        "pybind11>=2.6.0",
    ],
    extras_require={
        "dev": [
            "pytest>=6.0",
            "pytest-cov",
            "black",
            "flake8",
        ],
    },
    classifiers=[
        "Development Status :: 4 - Beta",
        "Intended Audience :: Science/Research",
        "Topic :: System :: Networking",
        "License :: OSI Approved :: MIT License",
        "Programming Language :: Python :: 3",
        "Programming Language :: Python :: 3.6",
        "Programming Language :: Python :: 3.7",
        "Programming Language :: Python :: 3.8",
        "Programming Language :: Python :: 3.9",
        "Programming Language :: Python :: 3.10",
        "Programming Language :: Python :: 3.11",
        "Programming Language :: C++",
    ],
    keywords="bgp routing simulator rov network-security",
    project_urls={
        "Source": "https://github.com/yourusername/bgp_simulator",
        "Bug Reports": "https://github.com/yourusername/bgp_simulator/issues",
    },
)
