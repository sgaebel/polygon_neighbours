/* CPython extension for finding neighouring polygons in parallel */

#if 1
  // local builds
  #include <python3.10/Python.h>
#else
  // docker build for wheel
  #include <Python.h>
#endif

// #define DEBUG_MAIN
// #define DBG_IDX_LOAD
// #define DBG_POLY_LOAD
// #define DEBUG_TASK
// #define DEBUG_LOADING

#include <vector>
#include <string>
#include <thread>
#include <algorithm>
#include <stdexcept>
#include <filesystem>
#include "cnpy.h"

#define PARALLEL
#define INPUT_FILE "/tmp/_temp_polygons_todo.npz"
#define OUTPUT_FILE_BASE "/tmp/_temp_polygon_neighbours_"


class Vertex {
    public:
        double x, y;

        Vertex(const double x, const double y)
        {
            this->x = x;
            this->y = y;
        }

        operator std::string() {
            return "[" + std::to_string(this->x) + ", "
                   + std::to_string(this->y) + "]";
        }

        const bool operator==(const Vertex &other) {
            return (this->x == other.x) && (this->y == other.y);
        }

        bool operator==(Vertex &other) {
            return (this->x == other.x) && (this->y == other.y);
        }

        const bool operator!=(const Vertex &other) {
            return (this->x != other.x) || (this->y != other.y);
        }

        bool operator!=(Vertex &other) {
            return (this->x != other.x) || (this->y != other.y);
        }
};


class PolygonEdge
{
    private:
        double x0, y0, x1, y1;

    public:
        PolygonEdge(double x0, double y0, double x1, double y1) {
            this->x0 = x0;
            this->y0 = y0;
            this->x1 = x1;
            this->y1 = y1;
        }

        PolygonEdge(const Vertex vertex_0, const Vertex vertex_1) {
            this->x0 = vertex_0.x;
            this->y0 = vertex_0.y;
            this->x1 = vertex_1.x;
            this->y1 = vertex_1.y;
        }

        const bool operator==(const PolygonEdge &other) {
            // if the start and end points match,
            //  flipped or not, the edge is shared
            if ((this->x0 == other.x0) &&
                (this->y0 == other.y0) &&
                (this->x1 == other.x1) &&
                (this->y1 == other.y1))
                return true;
            if ((this->x1 == other.x0) &&
                (this->y1 == other.y0) &&
                (this->x0 == other.x1) &&
                (this->y0 == other.y1))
                return true;
            return false;
        }

        const bool operator==(PolygonEdge &other) {
            // if the start and end points match,
            //  flipped or not, the edge is shared
            if ((this->x0 == other.x0) &&
                (this->y0 == other.y0) &&
                (this->x1 == other.x1) &&
                (this->y1 == other.y1))
                return true;
            if ((this->x1 == other.x0) &&
                (this->y1 == other.y0) &&
                (this->x0 == other.x1) &&
                (this->y0 == other.y1))
                return true;
            return false;
        }

        const std::string print() {
            std::string representation = "Edge: (";
            representation += std::to_string(this->x0) + ",";
            representation += std::to_string(this->y0) + "),(";
            representation += std::to_string(this->x1) + ",";
            representation += std::to_string(this->y1) + ")";
            return representation;
        }
};


class Polygon {
    private:
        std::vector<Vertex> vertices;

    public:
        Polygon(std::vector<Vertex> vertices)
        {
            if (vertices.front() != vertices.back()) {
                throw std::invalid_argument("Polygon is not closed.");
            }
            this->vertices = vertices;
        }

        const size_t size() {
            return this->vertices.size();
        }

        const Vertex operator[](const size_t idx) {
            return this->vertices.at(idx);
        }

        const std::vector<PolygonEdge> as_edges() {
            std::vector<PolygonEdge> edges;
            for (size_t idx = 0; idx < this->size()-1; ++idx)
                edges.push_back(PolygonEdge(this->vertices.at(idx),
                                            this->vertices.at(idx+1)));
            return edges;
        }

        const bool share_edge(Polygon &other) {
            std::vector<PolygonEdge> edges_self = this->as_edges();
            std::vector<PolygonEdge> edges_other = other.as_edges();
            for(size_t idx_self = 0; idx_self < edges_self.size(); ++idx_self)
                for(size_t idx_other = 0; idx_other < edges_other.size(); ++idx_other)
                    if(edges_self.at(idx_self) == edges_other.at(idx_other))
                        return true;
            return false;
        }
};


const std::vector<size_t> load_test_indices_from_npy(const std::string path)
{
    cnpy::NpyArray temp = cnpy::npz_load(path, "n_test_polygons");
    const size_t n_test_points = *temp.data<size_t>();
    #ifdef DBG_IDX_LOAD
    std::cout << "n_test_points = " << n_test_points << std::endl;
    #endif
    temp = cnpy::npz_load(path, "test_indices");
    size_t* idx_array = temp.data<size_t>();
    std::vector<size_t> test_indices;
    for(size_t idx = 0; idx < n_test_points; ++idx) {
        test_indices.push_back(idx_array[idx]);
        #ifdef DBG_IDX_LOAD
        std::cout << "test idx " << idx << "=" << idx_array[idx] << std::endl;
        #endif
    }
    return test_indices;
}


const std::vector<Polygon> load_polygons_from_npz(const std::string path)
{
    // first get the number of polygons
    cnpy::NpyArray temp = cnpy::npz_load(path, "n_polygons");
    const size_t n_polygons = *temp.data<size_t>();
    #ifdef DBG_POLY_LOAD
    std::cout << "n_polygons = " << n_polygons << std::endl;
    #endif
    // parse each polygon in order into this vector of polygons
    std::vector<Polygon> polygons;
    for(size_t idx = 0; idx < n_polygons; ++idx) {
        // define the key names for the polygons size and the polygon vertices
        const std::string length_key = "len_" + std::to_string(idx);
        const std::string data_key = "arr_" + std::to_string(idx);
        temp = cnpy::npz_load(path, length_key);
        const size_t length = *temp.data<size_t>();
        temp = cnpy::npz_load(path, data_key);
        double* data = temp.data<double>();
        std::vector<Vertex> polygon_data;
        for(size_t i = 0; i < length; ++i) {
            polygon_data.push_back(Vertex(data[2*i], data[2*i+1]));
            #ifdef DBG_POLY_LOAD
            std::cout << "loaded vertex: " << std::string(polygon_data.back()) << std::endl;
            #endif
        }
        if (polygon_data.front() != polygon_data.back()) {
            const std::string error = "Polygon "+std::to_string(idx)+" is not closed.";
            throw std::invalid_argument(error);
        }
        polygons.push_back(Polygon(polygon_data));
    }
    return polygons;
}


// load the input data and make it available globally
std::vector<size_t> test_indices;
size_t n_test_indices;
std::vector<Polygon> polygons;
size_t n_polygons;


// to refactor / test
//  Idea: have each thread write result to file. Maybe run more indices at once
//    and check progress via python thread
void find_neighbours_task(const size_t thread_idx)
{
    if(thread_idx >= test_indices.size())
        return;
    const size_t test_polygon_idx = test_indices.at(thread_idx);

    #ifdef DEBUG_TASK
    std::cout << "CHECKPOINT 0 " << test_polygon_idx << " :: " << thread_idx << std::endl << std::flush;
    #endif

    #ifdef DEBUG_TASK
    std::cout << "POLYGON ACCESS: " << thread_idx << " :: " << test_polygon_idx << std::endl << std::flush;
    #endif
    Polygon test_polygon = polygons.at(test_polygon_idx);

    #ifdef DEBUG_TASK
    std::cout << "CHECKPOINT 1" << std::endl << std::flush;
    #endif

    // loop through the other polygons searching for a neighouring polygon
    std::vector<long> matches;
    for (size_t idx = 0; idx < polygons.size(); ++idx) {
        if (idx == test_polygon_idx)
            // do not check for shared edges with itself
            continue;
        #ifdef DEBUG_TASK
        std::cout << "CHECKPOINT 2" << std::endl << std::flush;
        #endif
        const bool share_edge = test_polygon.share_edge(polygons.at(idx));
        if (share_edge) {
            // add the currently checked index to the matches
            matches.push_back(idx);
            #ifdef DEBUG_TASK
            std::cout << "LOOP match " << idx << std::endl << std::flush;
            #endif
        }
        #ifdef DEBUG_TASK
        else {
            std::cout << "LOOP no match " << idx << std::endl << std::flush;
        }
        std::cout << "CHECKPOINT 3" << std::endl << std::flush;
        #endif
    }
    if (matches.size() == 0)
        matches.push_back(-1);
    // write results to disk
    std::string out_path = OUTPUT_FILE_BASE + std::to_string(thread_idx) + ".npy";
    #ifdef DEBUG_TASK
    std::cout << "Output file name: " << out_path << std::endl << std::flush;
    #endif
    cnpy::npy_save(out_path, &matches[0], {matches.size()}, "w");
    return;
}


#ifdef __cplusplus
extern "C" {
#endif
static PyObject* find_neighbours(PyObject* self, PyObject* args)
{
    // no arguments to parse, all input via .npy & .npz files.
    if (std::filesystem::exists(std::filesystem::path(INPUT_FILE))) {
        test_indices = load_test_indices_from_npy(INPUT_FILE);
        n_test_indices = test_indices.size();
        polygons = load_polygons_from_npz(INPUT_FILE);
        n_polygons = polygons.size();
    }
    else {
        std::string message = "Input file \"";
        message += INPUT_FILE;
        message +=  "\" not found.";
        PyErr_SetString(PyExc_FileNotFoundError, message.c_str());
        return NULL;
    }
    // std::cout << "Blubb" << std::endl << std::flush;

    // tasks for this function:
    //  * run tasks and manage threads
    
    // test indices ought to be equal or multiple of the number of hardware
    //  threads
    const size_t N_THREADS = std::thread::hardware_concurrency();
    const size_t N_THREADS_USED = (N_THREADS > n_test_indices) ? n_test_indices : N_THREADS;

    #ifdef PARALLEL
    // keep record of threads
    std::vector<std::thread> threads;
    // threads are launched, then this 'main' thread runs one task...
    for (size_t idx = 1; idx < N_THREADS_USED; ++idx) {
        threads.push_back(std::thread(find_neighbours_task, idx));
    }
    // ...and joins the threads afterwards.
    find_neighbours_task(0);
    for (size_t idx = 0; idx < threads.size(); ++idx) {
        threads.at(idx).join();
    }
    #else
    // for testing purposes, run tasks in order in the main thread
    for (size_t idx = 0; idx < n_test_indices; ++idx) {
        #ifdef DEBUG_MAIN
        std::cout << "Running task " << idx << "... ";
        #endif
        find_neighbours_task(idx);
        #ifdef DEBUG_MAIN
        std::cout << "done." << std::endl;
        #endif
    }
    #endif

    Py_RETURN_NONE;
}


static PyMethodDef pt_methods[] = {
    { "find_neighbours", find_neighbours, METH_NOARGS,
    "Finds the neighbours of polygons within a large set of polygons.\n"
    "Polygons are read from 'data/polygons_todo.npz' and neighours\n"
    "are written to 'data/neighbours.npy.\n"
    "The input file is expected to contain arrays under keys of form\n"
    "'arr_0', 'arr_1', etc. The output file will contain a 2d array with\n"
    "the indices of neighbouring polygons. The polygon indices for\n"
    "which neighbours should be searched are expected to be passed\n"
    "within the input .npz file unser the keys 'n_test_polygons' and\n"
    "'test_indices'.\n"
    "\n"
    "\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "None\n"
    "\n"
    "Returns\n"
    "-------\n"
    "None\n"
    "\n"
    "Raises\n"
    "------\n"
    "\n"
    "\n" },
    { NULL, NULL, 0, NULL }
};


static struct PyModuleDef pt_module = {
    PyModuleDef_HEAD_INIT,
    "polygon_neighbours",
    NULL,
    -1,
    pt_methods
};


PyMODINIT_FUNC PyInit_polygon_neighbours(void)
{
    return PyModule_Create(&pt_module);
}

#ifdef __cplusplus
}  // closes extern "C"
#endif
