#ifndef HALIDE_HALIDERUNTIME_H
#define HALIDE_HALIDERUNTIME_H

#ifdef COMPILING_HALIDE
#include "mini_stdint.h"
#else
#include <stddef.h>
#include <stdint.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/** \file
 *
 * This file exports all routines which can be replaced by an
 * application hosting code generated by %Halide. These are used when
 * doing Ahead Of Time (AOT) compilation and must be supplied to the
 * linker to override a routine. I.e., just define your own version of
 * any of these functions with extern "C" linkage, and it should
 * replace the default one.
 *
 * When doing Just In Time (JIT) compilation methods on the Func being
 * compiled must be called instead. The corresponding methods are
 * documented below.
 *
 * All of these functions take a "void *user_context" parameter as their
 * first argument; if the Halide kernel that calls back to any of these
 * funcions has been defined with a "__user_context" parameter XXXXXXXX,
 * then the value of that pointer passed from the code that calls the
 * Halide kernel is piped through to the function.
 *
 * Some of these are also useful to call when using the default
 * implementation. E.g. halide_shutdown_thread_pool.
 *
 * Note that some linker setups may not respect the override you
 * provide. E.g. if the override is in a shared library and the halide
 * object files are linked directly into the output, the builtin
 * versions of the runtime functions will be called. See your linker
 * documentation for more details. On Linux, LD_DYNAMIC_WEAK=1 may
 * help.
 *
 */

/** Define halide_printf to catch debugging output, informational
  * messages, etc. Main use is to support HL_TRACE functionality and
  * PrintStmt in IR. Also called by the default halide_error
  * implementation.
  *
  * Do not replace this function, replace halide_print instead.
  */
extern int halide_printf(void *user_context, const char *, ...);

/** Unformatted print used to support halide_printf. */
extern void halide_print(void *user_context, const char *);

/** Define halide_error to catch errors messages at runtime, for
 * example bounds checking failures. Per the description above, use
 * halide_set_error_handler in JITted code and provide an
 * implementation of halide_error in AOT code.  See
 * Func::set_error_handler.
 */
//@{
extern void halide_error(void *user_context, const char *);
extern void halide_error_varargs(void *user_context, const char *, ...);
//@}

/** A macro that calls halide_error if the supplied condition is false. */
#define halide_assert(user_context, cond) if (!(cond)) halide_error(user_context, #cond);

/** Define halide_do_par_for to replace the default thread pool
 * implementation. halide_shutdown_thread_pool can also be called to
 * release resources used by the default thread pool on platforms
 * where it makes sense. (E.g. On Mac OS, Grand Central Dispatch is
 * used so %Halide does not own the threads backing the pool and they
 * cannot be released.)  See Func::set_custom_do_task and
 * Func::set_custom_do_par_for. Should return zero if all the jobs
 * return zero, or an arbitrarily chosen return value from one of the
 * jobs otherwise.
 */
//@{
extern int halide_do_par_for(void *user_context,
                             int (*f)(void *ctx, int, uint8_t *),
                             int min, int size, uint8_t *closure);
extern void halide_shutdown_thread_pool();
//@}

/** Define halide_malloc and halide_free to replace the default memory
 * allocator.  See Func::set_custom_allocator. (Specifically note that
 * halide_malloc must return a 32-byte aligned pointer, and it must be
 * safe to read at least 8 bytes before the start and beyond the
 * end.)
 */
//@{
extern void *halide_malloc(void *user_context, size_t x);
extern void halide_free(void *user_context, void *ptr);
//@}

/** Called when debug_to_file is used inside %Halide code.  See
 * Func::debug_to_file for how this is called
 *
 * Cannot be replaced in JITted code at present.
 */
extern int32_t halide_debug_to_file(void *user_context, const char *filename,
                                    uint8_t *data, int32_t s0, int32_t s1, int32_t s2,
                                    int32_t s3, int32_t type_code,
                                    int32_t bytes_per_element);


enum halide_trace_event_code {halide_trace_load = 0,
                              halide_trace_store = 1,
                              halide_trace_begin_realization = 2,
                              halide_trace_end_realization = 3,
                              halide_trace_produce = 4,
                              halide_trace_update = 5,
                              halide_trace_consume = 6,
                              halide_trace_end_consume = 7};

struct halide_trace_event {
    const char *func;
    halide_trace_event_code event;
    int32_t parent_id;
    int32_t type_code;
    int32_t bits;
    int32_t vector_width;
    int32_t value_index;
    void *value;
    int32_t dimensions;
    int32_t *coordinates;
};

/** Called when Funcs are marked as trace_load, trace_store, or
 * trace_realization. See Func::set_custom_trace. The default
 * implementation either prints events via halide_printf, or if
 * HL_TRACE_FILE is defined, dumps the trace to that file in a
 * yet-to-be-documented binary format (see src/runtime/tracing.cpp to
 * reverse engineer the format). If the trace is going to be large,
 * you may want to make the file a named pipe, and then read from that
 * pipe into gzip.
 *
 * halide_trace returns a unique ID which will be passed to future
 * events that "belong" to the earlier event as the parent id. The
 * ownership hierarchy looks like:
 *
 * begin_realization
 *    produce
 *      store
 *      update
 *      load/store
 *      consume
 *      load
 *      end_consume
 *    end_realization
 *
 * Threading means that ownership cannot be inferred from the ordering
 * of events. There can be many active realizations of a given
 * function, or many active productions for a single
 * realization. Within a single production, the ordering of events is
 * meaningful.
 */
extern int32_t halide_trace(void *user_context, const halide_trace_event *event);

/** If tracing is writing to a file. This call closes that file
 * (flushing the trace). Returns zero on success. */
extern int halide_shutdown_trace();

/** Set the seed for the random number generator used by
 * random_float. Also clears all other internal state for the random
 * number generator. */
extern void halide_set_random_seed(uint32_t seed);


/** Release all data associated with the current GPU backend, in particular
 * all resources (memory, texture, context handles) allocated by Halide. Must
 * be called explicitly when using AOT compilation. */
extern void halide_release(void *user_context);

/** Copy image data from device memory to host memory. This must be called
 * explicitly to copy back the results of a GPU-based filter. */
extern int halide_copy_to_host(void *user_context, struct buffer_t *buf);

/** Copy image data from host memory to device memory. This should not be
 * called directly; Halide handles copying to the device automatically. */
extern int halide_copy_to_dev(void *user_context, struct buffer_t *buf);

/** Wait for current GPU operations to complete. Calling this explicitly
 * should rarely be necessary, except maybe for profiling. */
extern int halide_dev_sync(void *user_context);

extern int halide_dev_malloc(void *user_context, struct buffer_t *buf);
extern int halide_dev_free(void *user_context, struct buffer_t *buf);
extern void *halide_init_kernels(void *user_context, void *state_ptr,
                                 const char *src, int size);
extern int halide_dev_run(void *user_context,
                          void *state_ptr,
                          const char *entry_name,
                          int blocksX, int blocksY, int blocksZ,
                          int threadsX, int threadsY, int threadsZ,
                          int shared_mem_bytes,
                          size_t arg_sizes[],
                          void *args[]);

#ifdef __cplusplus
} // End extern "C"
#endif

#endif // HALIDE_HALIDERUNTIME_H

