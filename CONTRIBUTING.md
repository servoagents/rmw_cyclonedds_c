# Contributing

Please open an issue before starting a large change. Bug fixes and focused
improvements can go directly to a pull request.

Keep changes inside the documented fixed-size, best-effort profile unless the
pull request also updates the contract tests and support table. New RMW entry
points must either implement their ROS 2 contract or return an explicit
unsupported result; they must not report success without doing the work.

Before submitting a change, run:

```sh
scripts/check.sh
scripts/test.sh
```

C and C++ files follow the repository `.clang-format` file. Python follows
PEP 8, shell scripts must pass `bash -n` and ShellCheck, and CMake files use
two-space indentation.

By contributing, you agree that your work is licensed under Apache-2.0.
