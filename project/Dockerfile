FROM postgres:18 AS builder

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y postgresql-server-dev-18 \
    build-essential cmake git curl pkg-config autoconf libtool libkrb5-dev \
    libssl-dev postgresql-server-dev-all zlib1g-dev

WORKDIR /build

RUN export CFLAGS="-fno-stack-protector -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 -fPIC"
RUN export CPPFLAGS="-fno-stack-protector -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0"
RUN export LDFLAGS="-fno-stack-protector"

RUN git clone https://github.com/yaml/libyaml.git --depth 1 -b release/0.2.5 && \
    git clone https://libwebsockets.org/repo/libwebsockets --depth 1 -b v4.3.3 && \
    git clone https://github.com/curl/curl.git --depth 1 && \
    git clone --recursive https://github.com/kubernetes-client/c.git && \
    git clone https://github.com/openssl/openssl --depth 1 -b openssl-3.0.13

RUN cd libyaml && mkdir build && cd build && \
    cmake -DCMAKE_C_FLAGS="-fno-stack-protector -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0" \
    -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DBUILD_SHARED_LIBS=OFF .. && \
    make install

RUN cd libwebsockets && mkdir build && cd build && \
    cmake -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
      -DCMAKE_C_FLAGS="-U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 -fno-stack-protector" \
      -DLWS_WITH_STATIC=ON \
      -DLWS_WITH_SHARED=OFF \
      -DLWS_WITHOUT_TESTAPPS=ON \
      -DLWS_WITH_COPROCESSES=OFF \
      -DLWS_WITH_SEQUENCER=OFF \
      -DCMAKE_INSTALL_PREFIX=/usr/local .. && \
    make install

RUN cd curl && ./buildconf && \
    ./configure --disable-shared --enable-static --with-pic \
            --without-libpsl --without-libssh2 --without-libssh \
            --without-gssapi --with-openssl --disable-threaded-resolver \
            CFLAGS="-U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 -fno-stack-protector" && \
    make -j$(nproc) install

RUN cd c/kubernetes && mkdir build && cd build && \
    cmake -DCMAKE_C_FLAGS="-fno-stack-protector -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0" \
      -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DBUILD_SHARED_LIBS=OFF .. && \
    make install

RUN cd openssl && \
    ./config no-shared no-async -fPIC -fno-stack-protector \
         -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 && \
    make -j$(nproc) && \
    make install_sw

COPY . /app
WORKDIR /app

RUN make clean && make

FROM postgres:18
COPY --from=builder /app/auth_k8s.so /usr/lib/postgresql/18/lib/
