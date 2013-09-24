
/*
 * This file was automatically generated using swig_doc.py.
 *
 * Any changes to it will be lost next time it is regenerated.
 */




%feature("docstring") gr::router::child "<+description of block+>"

%feature("docstring") gr::router::child::make "Return a shared_ptr to a new instance of router::child.

To avoid accidental use of raw pointers, router::child's constructor is in a private implementation class. router::child::make is the public interface for creating new instances.

Params: (n, child_index, hostname, in_queue, out_queue)"

%feature("docstring") gr::router::queue_sink "<+description of block+>"

%feature("docstring") gr::router::queue_sink::make "Return a shared_ptr to a new instance of router::queue_sink.

To avoid accidental use of raw pointers, router::queue_sink's constructor is in a private implementation class. router::queue_sink::make is the public interface for creating new instances.

Params: (item_size, shared_queue, index, preserve_index)"

%feature("docstring") gr::router::queue_source "<+description of block+>"

%feature("docstring") gr::router::queue_source::make "Return a shared_ptr to a new instance of router::queue_source.

To avoid accidental use of raw pointers, router::queue_source's constructor is in a private implementation class. router::queue_source::make is the public interface for creating new instances.

Params: (item_size, shared_queue, indexes, preserve_index, order)"

%feature("docstring") gr::router::root "<+description of block+>"

%feature("docstring") gr::router::root::make "Return a shared_ptr to a new instance of router::root.

To avoid accidental use of raw pointers, router::root's constructor is in a private implementation class. router::root::make is the public interface for creating new instances.

Params: (number_of_children, in_queue, out_queue)"