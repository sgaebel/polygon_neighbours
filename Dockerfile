FROM quay.io/pypa/manylinux_2_28_x86_64
ADD . /src
RUN mkdir /output
WORKDIR /src

# python3.10 -m build --sdist --outdir /output
# python3.10 -m build --outdir /output
# auditwheel repair /output/*whl -w /output
# rm /output/*-linux_*
# docker container ls
# docker cp romantic_euclid:/output dist

