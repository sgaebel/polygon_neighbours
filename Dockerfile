FROM quay.io/pypa/manylinux_2_28_x86_64
ADD . /src
RUN mkdir /output
ENV CPLUS_INCLUDE_PATH=/src/src/polygon_neighbours
WORKDIR /src
RUN sed -i 's/LOCAL_BUILD/DOCKER_BUILD/' src/polygon_neighbours/find_neighbours.cpp
ENTRYPOINT [ "bash", "scripts/build--docker.sh" ]
