import os
import numpy as np
import time
import multiprocessing

import polygon_neighbours


N_CPU = multiprocessing.cpu_count()


def regular_polygon(n_vertices, x_offset, y_offset):
    assert n_vertices > 2
    angles = np.linspace(0., 2*np.pi, n_vertices+1)
    xs, ys = np.sin(angles) + x_offset, np.cos(angles) + y_offset
    coord_pairs = list(zip(xs, ys))
    coord_pairs[-1] = coord_pairs[0]
    return np.array(coord_pairs)


def polygon_setup(N_polygons, N_test):
    assert N_test >= 3
    data = {'n_polygons': N_polygons,
            'arr_0': np.array([(0., -1.), (1., 0.), (0., 1.), (-1., 0.), (0., -1.)]),
            'len_0' : 5,
            'arr_1': np.array([(1., 0.), (3., 2.), (2., 3.), (0., 1.), (1., 0.)]),
            'len_1' : 5,
            'arr_2': np.array([(3., 2.), (2., 3.), (3., 4.), (4., 3.), (3., 2.)]),
            'len_2' : 5,
            'arr_3': np.array([(1., 0.), (3., 2.), (3., 0.), (1., 0.)]),
            'len_3' : 4,
            'arr_4': np.array([(3., 2.), (5., 1.5), (3., 0.), (3., 2.)]),
            'len_4' : 4,
            'arr_5': np.array([(1.5, 4.), (0., 4.), (0., 2.), (1.5, 2.), (1.5, 4.)]),
            'len_5' : 5,
            'test_indices': [1, 4, 5]}
    if N_test > 3:
        for _ in range(3, N_test):
            data['test_indices'].append(np.random.randint(4, N_polygons))
    data['n_test_polygons'] = len(data['test_indices'])
    # add some random polygons which are not expected to share any edges.
    for i in range(6, N_polygons):
        N = np.random.randint(3, 15)
        x0, y0 = np.random.standard_normal(size=2)
        data[f'len_{i}'] = N + 1  # closed
        data[f'arr_{i}'] = regular_polygon(N, x0, y0)
    try:
        assert len(data) == (2 * N_polygons + 3)
    except:
        print(len(data))
        print(list(data.keys()))
        raise
    for i in range(N_polygons):
        try:
            assert data[f'len_{i}'] == data[f'arr_{i}'].shape[0]
        except KeyError:
            print(*sorted(list(data.keys())), sep='\n')
            raise
        assert data[f'arr_{i}'].shape[1] == 2
    assert data['n_test_polygons'] == N_test, repr(N_test)
    assert len(data['test_indices']) == N_test, repr(N_test)
    np.savez('/tmp/_temp_polygons_todo.npz', **data)
    # print('Save done.', flush=True)
    time.sleep(0.2)
    return


def cleanup(N_test):
    time.sleep(0.2)
    os.remove('/tmp/_temp_polygons_todo.npz')
    for i in range(min(N_CPU, N_test)):
        os.remove(f'/tmp/_temp_polygon_neighbours_{i}.npy')
    # print('Test files removed.')
    return


def test_neighbours_0():
    # test c++ extension directly
    N_polygons = 6
    N_test = 3
    polygon_setup(N_polygons=N_polygons, N_test=N_test)
    polygon_neighbours.find_neighbours()
    # expected:
    #   1 -> (0, 2, 3)
    #   4 -> (3,)
    #   5 -> (,)
    expected = [np.array([-1])] * N_test
    expected[0] = np.array([0, 2, 3])
    expected[1] = np.array([3,])
    for i in range(N_test):
        neighbours = np.load(f'/tmp/_temp_polygon_neighbours_{i}.npy')
        assert np.all(expected[i] == neighbours)
    cleanup(N_test=N_test)
    return

def test_neighbours_1():
    # test c++ extension directly
    N_polygons = 25
    N_test = N_CPU
    polygon_setup(N_polygons=N_polygons, N_test=N_test)
    polygon_neighbours.find_neighbours()
    # expected:
    #   1 -> (0, 2, 3)
    #   4 -> (3,)
    #   5 -> (,)
    expected = [np.array([-1])] * N_test
    expected[0] = np.array([0, 2, 3])
    expected[1] = np.array([3,])
    for i in range(N_test):
        neighbours = np.load(f'/tmp/_temp_polygon_neighbours_{i}.npy')
        assert np.all(expected[i] == neighbours)
    cleanup(N_test=N_test)
    return
