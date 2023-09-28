# Polygon Neighbours

CPython extension for parallelised search of bordering polygons.
Currently the interface is cumbersome as the called C extension function
reads its input data purely from a temporary .npz file, and "returns"
through temporary .npy files.


## Example

```python
import polygon_neighbours
import numpy as np
import os

# preparing the input data
data = {'n_polygons': 6,
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
        'arr_5': np.array([(1.5, 4.), (0., 4.), (0., 2.), (1.5, 2.), (15, 4.)]),
        'len_5' : 5,
        'test_indices': [1, 4, 5],
        'n_test_polygons': 3}
# save to .npz file
np.savez('/tmp/_temp_polygons_todo.npz', **data)

polygon_neighbours.find_neighbours()

# read results from a seperate file for each thread/test index
neighbouring_indices = {}
for test_idx in range(data['test_indices']):
    neighbouring_indices[test_idx] = np.load(f'/tmp/_temp_polygon_neighbours_{test_idx}.npy')
# {1: ndarray([0, 2, 3]),
#  4: ndarray([3,]),
#  5: ndarray([])}

# cleanup
for test_idx in range(data['test_indices']):
    os.remove(f'/tmp/_temp_polygon_neighbours_{test_idx}.npy')
os.remove('/tmp/_temp_polygons_todo.npz')
```
