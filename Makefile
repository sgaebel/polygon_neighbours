clean:
	rm -rf .pytest_cache/ polygon_neighbours.cpython-310-x86_64-linux-gnu.so polygon_neighbours.egg-info/ build/
	yes | pip3 uninstall polygon_neighbours || true

install:
	pip3 install --user .

test: clean install
	pytest

performance: clean install
	python3 scripts/check_performance.py

performance-help:
	echo "use 'python3 scripts/check_performance.py --help' for custom profiling options."

wheel:
	sed -i 's/LOCAL_BUILD/DOCKER_BUILD/' src/polygon_neighbours/find_neighbours.cpp
	python3.10 -m build --outdir /output
	python3.10 -m build --sdist --outdir /output
	auditwheel repair /output/*whl -w /output
	yes | rm /output/*-linux_*
