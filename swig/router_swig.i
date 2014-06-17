/* -*- c++ -*- */

#define ROUTER_API

%include "gnuradio.i"			// the common stuff

//load generated python docstrings
%include "router_swig_doc.i"

%{
#include "router/child.h"
#include "router/root.h"
#include "router/queue_sink.h"
#include "router/queue_source.h"
#include "router/test.h"
#include "router/throughput.h"
#include "router/throughput_sink.h"
#include "router/queue_sink_byte.h"
#include "router/queue_source_byte.h"
%}


%include "router/child.h"
GR_SWIG_BLOCK_MAGIC2(router, child);
%include "router/root.h"
GR_SWIG_BLOCK_MAGIC2(router, root);

%include "router/queue_sink.h"
GR_SWIG_BLOCK_MAGIC2(router, queue_sink);
%include "router/queue_source.h"
GR_SWIG_BLOCK_MAGIC2(router, queue_source);
%include "router/test.h"
%include "router/throughput.h"
GR_SWIG_BLOCK_MAGIC2(router, throughput);
%include "router/throughput_sink.h"
GR_SWIG_BLOCK_MAGIC2(router, throughput_sink);
%include "router/queue_sink_byte.h"
GR_SWIG_BLOCK_MAGIC2(router, queue_sink_byte);
%include "router/queue_source_byte.h"
GR_SWIG_BLOCK_MAGIC2(router, queue_source_byte);
