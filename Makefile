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

