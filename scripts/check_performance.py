
import click
import os
import numpy as np
import time
import multiprocessing
import random
import sys

import polygon_neighbours


N_CPU = multiprocessing.cpu_count()
del multiprocessing


def regular_polygon(n_vertices, x_offset, y_offset):
    assert n_vertices > 2
    angles = np.linspace(0., 2*np.pi, n_vertices+1)
    xs, ys = np.sin(angles) + x_offset, np.cos(angles) + y_offset
    coord_pairs = list(zip(xs, ys))
    coord_pairs[-1] = coord_pairs[0]
    return np.array(coord_pairs)


def format_bytes(size):
    # 2**10 = 1024
    power = 2**10
    n = 0
    power_labels = {0 : 'B', 1: 'kiB', 2: 'MiB', 3: 'GiB', 4: 'TiB'}
    while size > power:
        size /= power
        n += 1
    return f'{size:.2f} {power_labels[n]}'


@click.command('Run Benchmark')
@click.option('--parallel/--serial', default=True,
              help="Toggles parallelism.")
@click.option('--n-polygons', '-p', type=int, default=4200,
              help="Number of polygons used for profiling.")
@click.option('--n-test', '-t', type=int, default=N_CPU,
              help="Number of polygons to test.")
@click.option('--edge-range', nargs=2, type=int, default=(24, 420),
              help="Minmum and maximum number of edges of the "
                   "generated polygons.")
def test_performance(parallel, n_polygons, n_test, edge_range):
    """Benchmarks the performance of 'polygon_neighbours.find_neighbours'
    via randomly generated polygons (which most like will not share any edges).
    """
    if n_polygons < n_test:
        raise ValueError(f'Cannot test (N={n_test}) more polygons than there '
                         f'are polygons (N={n_polygons}).')
    print(f'''Running performance test with:
    {n_polygons=}
    {n_test=}
    {edge_range=}
    Parallelism={parallel}
''')
    data = {}
    polygon_sizes = []
    for idx in range(n_polygons):
        n_vertices = np.random.randint(*edge_range) + 1
        polygon_sizes.append(n_vertices)
        x_offset, y_offset = np.random.normal(scale=5.4, size=2)
        polygon = regular_polygon(n_vertices, x_offset, y_offset)
        data[f'arr_{idx}'] = np.array(polygon, dtype=np.float64)
        data[f'len_{idx}'] = len(polygon)
    data['n_polygons'] = n_polygons
    data['test_indices'] = random.sample(population=range(n_polygons), k=n_test)
    data['n_test_polygons'] = n_test
    print(f'Approx. data size: {format_bytes(sys.getsizeof(data))}')
    ###################
    np.savez('/tmp/_temp_polygons_todo.npz', **data)
    ###################
    print(f'\tTest start: [{"PARALLEL" if parallel else "SERIAL"}]')
    t0 = time.perf_counter()
    polygon_neighbours.find_neighbours(n_polygons // 25, True, parallel)
    dt = time.perf_counter() - t0
    print(f'Time per polygon [N={n_polygons}]: {1000 * dt / n_polygons:.3f}ms')
    print(f'Avg. polygon size: {np.mean(polygon_sizes):.2f}')
    ###################
    time.sleep(0.2)
    os.remove('/tmp/_temp_polygons_todo.npz')
    for i in range(min(N_CPU, n_test)):
        os.remove(f'/tmp/_temp_polygon_neighbours_{i}.npy')
    return


if __name__ == '__main__':
    test_performance()
