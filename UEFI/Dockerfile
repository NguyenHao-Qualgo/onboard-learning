FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    git python3 python3-setuptools python3-pip wget curl ca-certificates \
    build-essential uuid-dev gcc-12 g++-12 \
    virtualenv \
    gcc-12-aarch64-linux-gnu g++-12-aarch64-linux-gnu \
    device-tree-compiler \
    mono-devel mono-utils \
    lcov \
  && update-ca-certificates \
  && cert-sync /etc/ssl/certs/ca-certificates.crt || true \
  && rm -rf /var/lib/apt/lists/*

RUN update-alternatives \
      --install /usr/bin/gcc gcc /usr/bin/gcc-12 100 \
      --slave /usr/bin/g++ g++ /usr/bin/g++-12 \
      --slave /usr/bin/gcc-ar gcc-ar /usr/bin/gcc-ar-12 \
      --slave /usr/bin/gcc-nm gcc-nm /usr/bin/gcc-nm-12 \
      --slave /usr/bin/gcc-ranlib gcc-ranlib /usr/bin/gcc-ranlib-12 \
      --slave /usr/bin/gcov gcov /usr/bin/gcov-12 \
  && update-alternatives --install /usr/bin/cpp cpp /usr/bin/cpp-12 100

RUN update-alternatives \
      --install /usr/bin/aarch64-linux-gnu-gcc aarch64-linux-gnu-gcc /usr/bin/aarch64-linux-gnu-gcc-12 100 \
      --slave /usr/bin/aarch64-linux-gnu-g++ aarch64-linux-gnu-g++ /usr/bin/aarch64-linux-gnu-g++-12 \
      --slave /usr/bin/aarch64-linux-gnu-gcc-ar aarch64-linux-gnu-gcc-ar /usr/bin/aarch64-linux-gnu-gcc-ar-12 \
      --slave /usr/bin/aarch64-linux-gnu-gcc-nm aarch64-linux-gnu-gcc-nm /usr/bin/aarch64-linux-gnu-gcc-nm-12 \
      --slave /usr/bin/aarch64-linux-gnu-gcc-ranlib aarch64-linux-gnu-gcc-ranlib /usr/bin/aarch64-linux-gnu-gcc-ranlib-12 \
      --slave /usr/bin/aarch64-linux-gnu-gcov aarch64-linux-gnu-gcov /usr/bin/aarch64-linux-gnu-gcov-12 \
  && update-alternatives --install /usr/bin/aarch64-linux-gnu-cpp aarch64-linux-gnu-cpp /usr/bin/aarch64-linux-gnu-cpp-12 100

# Non-root user
ARG USERNAME=edk2
ARG USER_UID=1000
ARG USER_GID=1000
RUN groupadd --gid ${USER_GID} ${USERNAME} \
  && useradd --uid ${USER_UID} --gid ${USER_GID} -m -s /bin/bash ${USERNAME}

RUN apt-get update && apt-get install -y --no-install-recommends sudo \
  && rm -rf /var/lib/apt/lists/*

RUN usermod -aG sudo ${USERNAME} \
  && echo "${USERNAME} ALL=(ALL) NOPASSWD:ALL" > /etc/sudoers.d/${USERNAME} \
  && chmod 0440 /etc/sudoers.d/${USERNAME}

USER ${USERNAME}
WORKDIR /work
RUN mkdir edkrepo && cd edkrepo \
  && wget https://github.com/tianocore/edk2-edkrepo/releases/download/edkrepo-v2.1.2/edkrepo-2.1.2.tar.gz \
  && tar xvf edkrepo-2.1.2.tar.gz \
  && sudo ./install.py --user ${USERNAME} --no-prompt \
  && sudo chown -R ${USERNAME}. ~/.edkrepo
#   && cd .. \
#   && edkrepo manifest-repos add nvidia https://github.com/NVIDIA/edk2-edkrepo-manifest.git main nvidia \
#   && edkrepo clone nvidia-uefi NVIDIA-Platforms main

ENV HOME=/home/${USERNAME}
CMD ["bash"]
