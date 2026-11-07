"""Adjust the local llvm-amdgpu build features used by the CI image."""

# Copyright 2013-2023 Lawrence Livermore National Security, LLC and other
# Spack Project Developers. See the top-level COPYRIGHT file for details.
#
# SPDX-License-Identifier: (Apache-2.0 OR MIT)

from spack.package import Spec, variant
from spack_repo.builtin.packages.llvm_amdgpu.package import LlvmAmdgpu as BuiltinLlvmAmdgpu


class LlvmAmdgpu(BuiltinLlvmAmdgpu):
    """Avoid building unused Fortran components for the LLVM 22 provider."""

    variant(
        "flang",
        default=True,
        description="Build the Flang compiler and runtime when supported",
    )

    @property
    def supported_languages(self):
        """Report the compiler languages enabled for this spec."""
        languages = ["c", "cxx"]
        if self.spec.satisfies("+flang"):
            languages.append("fortran")
        return languages

    def _fortran_path(self):
        """Return the AMD Fortran compiler only when Flang was built."""
        if not self.spec.satisfies("+flang"):
            return None
        return super()._fortran_path()

    def cmake_args(self):
        """Drop Fortran CMake components when the variant is disabled."""
        args = super().cmake_args()
        if self.spec.satisfies("~flang"):
            for variable, component in (
                ("LLVM_ENABLE_PROJECTS", "flang"),
                ("LLVM_ENABLE_RUNTIMES", "flang-rt"),
            ):
                prefix = f"-D{variable}="
                for index, argument in enumerate(args):
                    if argument.startswith(prefix):
                        components = argument.removeprefix(prefix).split(";")
                        components = [item for item in components if item != component]
                        args[index] = self.define(variable, components)
                        break
        return args


# The upstream recipe advertises Fortran from 7.0, but this override controls its availability.
_provided = {
    constraint: set(capabilities)
    for constraint, capabilities in BuiltinLlvmAmdgpu.provided.items()
}
_provided.pop(Spec("@7.0:"))
_provided[Spec("@7.0: +flang")] = {Spec("fortran")}
LlvmAmdgpu.provided = _provided
