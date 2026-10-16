FROM ubuntu:22.04
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential cmake curl ca-certificates libgtest-dev \
    libocct-data-exchange-dev libocct-modeling-algorithms-dev valgrind \
    && rm -rf /var/lib/apt/lists/*
COPY scripts/bootstrap_occt.sh /usr/local/bin/bootstrap_occt
RUN bash /usr/local/bin/bootstrap_occt /opt/bs2quad-deps 8 \
    && rm -rf /opt/bs2quad-deps/occt-build /opt/bs2quad-deps/OCCT-7_7_0
ENV OpenCASCADE_DIR=/opt/bs2quad-deps/occt770/lib/cmake/opencascade
ENV LD_LIBRARY_PATH=/opt/bs2quad-deps/occt770/lib
WORKDIR /workspace
COPY . /workspace
RUN cmake -S . -B /opt/bs2quad-build -DCMAKE_BUILD_TYPE=Release \
    && cmake --build /opt/bs2quad-build -j 4 \
    && ctest --test-dir /opt/bs2quad-build --output-on-failure
ENTRYPOINT ["/opt/bs2quad-build/bs2quad_main"]
CMD ["--help"]
