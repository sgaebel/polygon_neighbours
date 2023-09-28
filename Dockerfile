FROM quay.io/pypa/manylinux_2_28_x86_64
ADD . /src
RUN mkdir /output
ENV CPLUS_INCLUDE_PATH=/src/src/polygon_neighbours
WORKDIR /src

# python3.10 -m build --outdir /output
# python3.10 -m build --sdist --outdir /output
# auditwheel repair /output/*whl -w /output
# yes | rm /output/*-linux_*
# docker container ls
# docker cp <container_name>:/output dist
