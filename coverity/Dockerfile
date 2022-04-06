FROM ubuntu

ADD cov-analysis-linux64-2022.3.0.tar.gz /
RUN apt update && apt install -y \
    python3 \
    python3-pip \
    git \
    && rm -rf /var/lib/apt/lists/*
RUN pip3 install platformio
COPY license.dat /cov-analysis-linux64-2022.3.0/bin/
ENV PATH="/cov-analysis-linux64-2022.3.0/bin":${PATH}
RUN cov-configure --clang
RUN cov-configure --comptype gcc --template --compiler xtensa-esp32-elf-gcc
RUN mkdir /project

WORKDIR /project
