/* -*- c++ -*- */
/* 
 * Copyright 2013 <+YOU OR YOUR COMPANY+>.
 * 
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3, or (at your option)
 * any later version.
 * 
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this software; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street,
 * Boston, MA 02110-1301, USA.
 */


#ifndef INCLUDED_ROUTER_QUEUE_SOURCE_H
#define INCLUDED_ROUTER_QUEUE_SOURCE_H

#include <router/api.h>
#include <gnuradio/sync_block.h>
#include <queue>
#include <memory>
#include <boost/lockfree/queue.hpp>
#include <boost/thread.hpp>

namespace gr {
  namespace router {

    /*!
     * \brief <+description of block+>
     * \ingroup router
     *
     */
    class ROUTER_API queue_source : virtual public gr::sync_block
    {
    public:
       typedef boost::shared_ptr<queue_source> sptr;

       /*!
        * \brief Return a shared_ptr to a new instance of router::queue_source.
        *
        * To avoid accidental use of raw pointers, router::queue_source's
        * constructor is in a private implementation
        * class. router::queue_source::make is the public interface for
        * creating new instances.
        */
       static sptr make(int item_size, boost::shared_ptr< boost::lockfree::queue< std::vector<float>* > > shared_queue, boost::shared_ptr< boost::lockfree::queue<float> > indexes, bool preserve_index, bool order);
    };

  } // namespace router
} // namespace gr

#endif /* INCLUDED_ROUTER_QUEUE_SOURCE_H */

