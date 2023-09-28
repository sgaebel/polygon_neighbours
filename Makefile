clean:
	rm -rf .pytest_cache/ polygon_neighbours.cpython-310-x86_64-linux-gnu.so polygon_neighbours.egg-info/ build/
	yes | pip3 uninstall polygon_neighbours || true
	rm data/* || true

install:
	pip3 install --user .

test: clean install
	pytest
