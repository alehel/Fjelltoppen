# Cross-compiler for the Windows build, see build.sh
FROM docker.io/library/debian:trixie-slim
RUN apt-get update && apt-get install -y --no-install-recommends \
		ca-certificates curl make xz-utils bzip2 zip gcc libc6-dev \
		gcc-mingw-w64-x86-64-posix g++-mingw-w64-x86-64-posix binutils-mingw-w64-x86-64 \
	&& rm -rf /var/lib/apt/lists/*
