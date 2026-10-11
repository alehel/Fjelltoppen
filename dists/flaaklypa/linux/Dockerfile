# Build environment for the Linux AppImage, see build.sh. Ubuntu 22.04
# keeps the glibc requirement low (2.35).
FROM docker.io/library/ubuntu:22.04
ARG DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
		ca-certificates curl make xz-utils bzip2 file g++ pkg-config \
		libx11-dev libxext-dev libxrandr-dev libxcursor-dev libxfixes-dev libxi-dev libxss-dev \
		libxkbcommon-dev libwayland-dev wayland-protocols libdecor-0-dev libegl1-mesa-dev \
		libgl1-mesa-dev libgles2-mesa-dev libdrm-dev libgbm-dev \
		libasound2-dev libpulse-dev libpipewire-0.3-dev libdbus-1-dev libudev-dev \
	&& rm -rf /var/lib/apt/lists/*
RUN set -e; \
	case $(uname -m) in \
		x86_64) tool=ed4ce84f0d9caff66f50bcca6ff6f35aae54ce8135408b3fa33abfc3cb384eb0; \
		        runtime=2fca8b443c92510f1483a883f60061ad09b46b978b2631c807cd873a47ec260d ;; \
		aarch64) tool=f0837e7448a0c1e4e650a93bb3e85802546e60654ef287576f46c71c126a9158; \
		         runtime=00cbdfcf917cc6c0ff6d3347d59e0ca1f7f45a6df1a428a0d6d8a78664d87444 ;; \
	esac; \
	curl -fsSL -o /opt/appimagetool https://github.com/AppImage/appimagetool/releases/download/1.9.1/appimagetool-$(uname -m).AppImage; \
	curl -fsSL -o /opt/runtime https://github.com/AppImage/type2-runtime/releases/download/20251108/runtime-$(uname -m); \
	echo "$tool  /opt/appimagetool" | sha256sum -c -; \
	echo "$runtime  /opt/runtime" | sha256sum -c -; \
	chmod +x /opt/appimagetool
