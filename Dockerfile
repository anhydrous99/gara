FROM ubuntu:latest

WORKDIR /app

RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    libboost-all-dev \
    git \
    zlib1g-dev \
    libssl-dev \
    libcurl4-openssl-dev \
    libasio-dev \
    nlohmann-json3-dev


RUN git clone https://github.com/aws/aws-sdk-cpp.git  \
    && cd aws-sdk-cpp \
    && git submodule update --init --recursive \
    && mkdir build && cd build \
    && cmake .. -DCMAKE_BUILD_TYPE=Release -DBUILD_ONLY="s3;dynamodb;secretsmanager" \
    && make && make install && cd ../../ && rm -rf aws-sdk-cpp


RUN git clone https://github.com/CrowCpp/Crow.git \
    && cd Crow \
    && mkdir build && cd build \
    && cmake .. -DCROW_BUILD_EXAMPLES=OFF -DCROW_BUILD_TESTS=OFF -DCMAKE_BUILD_TYPE=Release \
    && make && make install && cd ../../ && rm -rf Crow


RUN git clone https://github.com/pantor/inja.git \
    && cd inja && mkdir build && cd build \
    && cmake .. -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF -DINJA_BUILD_TESTS=OFF -DCOVERALLS=OFF

COPY . .

RUN mkdir -p build && \
    cd build && \
    cmake .. -DCMAKE_BUILD_TYPE=Release && \
    make -j$(nproc) && \
    chmod +x gara

COPY /app/build/gara /usr/local/bin/gara
RUN chmod +x /usr/local/bin/gara

RUN useradd -r -s /bin/false crowuser

USER crowuser

EXPOSE 8080

HEALTHCHECK --interval=30s --timeout=3s --start-period=5s --retries=3 \
    CMD curl -f http://localhost:8080/health || exit 1

CMD ["gara"]