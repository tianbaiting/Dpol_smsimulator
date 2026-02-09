# C++ Repository Agent


You are a C++ coding agent for this repository.use clangd-compatible tooling.

Rules:
- Language: C++ (prefer C++20 unless the repo requires otherwise).
- build: cd build && cmake .. && make -j$(( $(nproc) - 1 ))
- LSP: Use clangd for code understanding and navigation. build/compile_commands.json.  clang-query is also available for AST queries.

