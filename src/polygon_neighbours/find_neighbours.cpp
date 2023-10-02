/* CPython extension for finding neighouring polygons in parallel */

// will be replaced be "DOCKER_BUILD" for the wheel building inside docker
#define LOCAL_BUILD

#ifdef DOCKER_BUILD
  // docker build for wheel
  #include <Python.h>
#else
  // local builds
  #include <python3.10/Python.h>
#endif

// #define DEBUG_MAIN
// #define DBG_IDX_LOAD
// #define DBG_POLY_LOAD
// #define DEBUG_TASK
// #define DEBUG_LOADING

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <thread>
#include <filesystem>
#include <numeric>
#include <stdexcept>
#include <string>
#include <vector>
#include "cnpy.h"

#define THREAD_DELAY_MS 50
#define INPUT_FILE "/tmp/_temp_polygons_todo.npz"
#define OUTPUT_FILE_BASE "/tmp/_temp_polygon_neighbours_"
#define data_type double
#define NL_flush std::endl << std::flush


class Vertex {
    public:
        data_type x, y;

        Vertex(const data_type x, const data_type y)
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
        data_type x0, y0, x1, y1;

    public:
        PolygonEdge(data_type x0, data_type y0, data_type x1, data_type y1) {
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


class Timer
{
    std::chrono::time_point<std::chrono::high_resolution_clock> start_time;
    std::chrono::time_point<std::chrono::high_resolution_clock> end_time;
    long long delta_t_ns = -1;
    const long long ns_per_µs = 1000;
    const long long ns_per_ms = 1000 * ns_per_µs;
    const long long ns_per_s = 1000 * ns_per_ms;
    const long long ns_per_min = 60 * ns_per_s;
    const long long ns_per_hour = 60 * ns_per_min;
    const int readability_factor = 100;
    public:
        Timer() {
            this->start_time = std::chrono::high_resolution_clock::now();
        }
        Timer(long long delta_t_ns) {
            this->delta_t_ns = delta_t_ns;
        }
        const long long elapsed() {
            return this->delta_t_ns;
        }
        const void end() {
            this->end_time = std::chrono::high_resolution_clock::now();
        }
        operator std::string() {
            if (this->delta_t_ns < 0) {
                this->delta_t_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(this->end_time - this->start_time).count();
            }
            const long long delta_t = this->delta_t_ns;
            std::string value, unit;
            if (delta_t > ns_per_hour * readability_factor) {  // hours
                       // ^ µs   ^ ms   ^ s    ^min ^h
                long long hours = div(delta_t, ns_per_hour).quot;
                long long minutes = div(delta_t, ns_per_hour).rem;
                std::string value = std::to_string(hours) + ':' + std::to_string(minutes);
                std::string unit = "h";
                return value + " " + unit;
            } else if (delta_t > ns_per_min * readability_factor) {
                long long minutes = div(delta_t, ns_per_min).quot;
                long long seconds = div(delta_t, ns_per_min).rem;
                std::string value = std::to_string(minutes) + ':' + std::to_string(seconds);
                std::string unit = "min";
                return value + " " + unit;
            } else if (delta_t > ns_per_s * readability_factor) {
                long long seconds = div(delta_t, ns_per_s).quot;
                long long millisec = div(delta_t, ns_per_s).rem;
                std::string value = std::to_string(seconds) + '.' + std::to_string(millisec/10);
                std::string unit = "sec";
                return value + " " + unit;
            } else if (delta_t > ns_per_ms * readability_factor) {
                long long millisec = div(delta_t, ns_per_ms).quot;
                long long microsec = div(delta_t, ns_per_ms).rem;
                std::string value = std::to_string(millisec) + '.' + std::to_string(microsec/10);
                std::string unit = "ms";
                return value + " " + unit;
            } else if (delta_t > ns_per_µs * readability_factor) {
                long long microsec = div(delta_t, ns_per_µs).quot;
                long long nanosec = div(delta_t, ns_per_µs).rem;
                std::string value = std::to_string(microsec) + '.' + std::to_string(nanosec/10);
                std::string unit = "µs";
                return value + " " + unit;
            } else {
                std::string value = std::to_string(delta_t);
                std::string unit = "ns";
                return value + " " + unit;
            }
            return NULL;
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
        data_type* data = temp.data<data_type>();
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


// ~~~*** GLOBALS ***~~~
// load the input data and make it available globally
std::vector<size_t> test_indices;
size_t n_test_indices;
std::vector<Polygon> polygons;
size_t n_polygons;
std::vector<std::unique_ptr<std::atomic_ulong>> n_processed;
unsigned long n_processed_serial = 0;
size_t N_THREADS_USED;
int progress_step_size = 0;
int verbose = 0;
int parallel = 1;
// ~~** END GLOBALS **~~


const unsigned long increment_get_processed(const size_t thread_idx)
{
    if (parallel)
        return ++(*n_processed.at(thread_idx));
    return ++n_processed_serial;
}


void print_progress_message()
{
    if (parallel) {
        for (size_t idx = 0; idx < N_THREADS_USED; ++idx) {
            unsigned long count = *n_processed.at(idx);
            std::string seperator;
            if (idx != 0) seperator = " ~ "; else seperator = "";
            std::cout << seperator << "T" << std::to_string(idx) << " = " << std::to_string(count);
        }
        std::cout << " / " << std::to_string(n_polygons) << "\r";
        return;
    }
    std::cout << "#" << n_processed_serial << " / " << n_polygons << "\r";
    return;
}


// to refactor / test
//  Idea: have each thread write result to file. Maybe run more indices at once
//    and check progress via python thread
void find_neighbours_task(const size_t thread_idx)
{
    if (verbose) {
        if (parallel) {
            std::this_thread::sleep_for(std::chrono::milliseconds(THREAD_DELAY_MS*thread_idx));
            std::cout << "Thread " << thread_idx+1 << " launched." << NL_flush;
            std::this_thread::sleep_for(std::chrono::milliseconds(THREAD_DELAY_MS*(N_THREADS_USED-thread_idx)));
        }
        else {
            std::cout << "Running test index " << thread_idx+1 << NL_flush;
        }
    }
        
    if(thread_idx >= test_indices.size())
        return;
    const size_t test_polygon_idx = test_indices.at(thread_idx);

    #ifdef DEBUG_TASK
    std::cout << "CHECKPOINT 0 " << test_polygon_idx << " :: " << thread_idx << NL_flush;
    #endif

    #ifdef DEBUG_TASK
    std::cout << "POLYGON ACCESS: " << thread_idx << " :: " << test_polygon_idx << NL_flush;
    #endif
    Polygon test_polygon = polygons.at(test_polygon_idx);

    #ifdef DEBUG_TASK
    std::cout << "CHECKPOINT 1" << NL_flush;
    #endif

    // loop through the other polygons searching for a neighouring polygon
    std::vector<long> matches;
    for (size_t idx = 0; idx < polygons.size(); ++idx) {
        if (idx == test_polygon_idx)
            // do not check for shared edges with itself
            continue;
        #ifdef DEBUG_TASK
        std::cout << "CHECKPOINT 2" << NL_flush;
        #endif
        const bool share_edge = test_polygon.share_edge(polygons.at(idx));
        if (share_edge) {
            // add the currently checked index to the matches
            matches.push_back(idx);
            #ifdef DEBUG_TASK
            std::cout << "LOOP match " << idx << NL_flush;
            #endif
        }
        #ifdef DEBUG_TASK
        else {
            std::cout << "LOOP no match " << idx << NL_flush;
        }
        std::cout << "CHECKPOINT 3" << NL_flush;
        #endif
        const unsigned long n_processed = increment_get_processed(thread_idx);
        if ((progress_step_size > 0)
            && (thread_idx == 0)
            && (n_processed % progress_step_size == 0))
            // std::cout << "~";
            print_progress_message();
    }
    if (matches.size() == 0)
        matches.push_back(-1);
    // write results to disk
    std::string out_path = OUTPUT_FILE_BASE + std::to_string(thread_idx) + ".npy";
    #ifdef DEBUG_TASK
    std::cout << "Output file name: " << out_path << NL_flush;
    #endif
    cnpy::npy_save(out_path, &matches[0], {matches.size()}, "w");
    if (thread_idx == 0)
        std::cout << std::endl << NL_flush;
    return;
}


#ifdef __cplusplus
extern "C" {
#endif
static PyObject* find_neighbours(PyObject* self, PyObject* args)
{
    if (!PyArg_ParseTuple(args, "|iii", &progress_step_size, &verbose,
                          &parallel))
        return NULL;

    if (std::filesystem::exists(std::filesystem::path(INPUT_FILE))) {
        if (verbose) {
            std::cout << "Loading data... " << std::flush;
        }
        Timer load_timer = Timer();
        test_indices = load_test_indices_from_npy(INPUT_FILE);
        n_test_indices = test_indices.size();
        polygons = load_polygons_from_npz(INPUT_FILE);
        n_polygons = polygons.size();
        load_timer.end();
        if (verbose) {
            std::cout << "done in " << std::string(load_timer) << NL_flush;
        }
        // test indices ought to be equal or multiple of the number of hardware
        //  threads
        const size_t N_THREADS = std::thread::hardware_concurrency();
        // use the small of the two values to avoid attemting to start
        //  threads without available work
        N_THREADS_USED = (N_THREADS > n_test_indices) ? n_test_indices : N_THREADS;
        // holds information about the progress of seach thread
        n_processed.resize(N_THREADS_USED);
        // initialise the progress counter to zero
        for (auto& ptr : n_processed)
            ptr = std::make_unique<std::atomic_ulong>(0);
    }
    else {
        std::string message = "Input file \"";
        message += INPUT_FILE;
        message +=  "\" not found.";
        PyErr_SetString(PyExc_FileNotFoundError, message.c_str());
        return NULL;
    }

    Timer run_timer = Timer();
    if (parallel) {
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
    } else {
        // for testing and comparison purposes, run tasks in order in the main thread
        for (size_t idx = 0; idx < N_THREADS_USED; ++idx) {
            #ifdef DEBUG_MAIN
            std::cout << "Running task " << idx << "... ";
            #endif
            find_neighbours_task(idx);
            #ifdef DEBUG_MAIN
            std::cout << "done." << std::endl;
            #endif
        }
    }
    run_timer.end();
    if (verbose) {
        std::cout << "Processing time: " << std::string(run_timer);
        std::cout << " ::: " << std::string(Timer(run_timer.elapsed() / n_polygons))
                << " per polygon" << NL_flush; 
    }

    std::cout << std::endl;

    Py_RETURN_NONE;
}


static PyMethodDef pt_methods[] = {
    { "find_neighbours", find_neighbours, METH_VARARGS,
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
    "Parameters\n"
    "----------\n"
    "progress_step_size: int\n"
    "    Interval at which progress is printed, in overall polygon counts.\n"
    "    i.e. after every x globally processed polygons, progress is printed.\n"
    "    If 0, no progress is printed. Default value is 0.\n"
    "verbose: bool | int\n"
    "    Toggles the verbosity to print some useful threading and timing\n"
    "    information. Adds a ~100ms delay to every call. Default value is False.\n"
    "parallel: bool | int\n"
    "    Toggles if the search shoul be run in parallel. Default value is True.\n"
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
