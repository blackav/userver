
//          Copyright Oliver Kowalke 2014.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_COROUTINES2_FIXEDSIZE_H
#define BOOST_COROUTINES2_FIXEDSIZE_H

#include <exception>

#include <boost/assert.hpp>
#include <boost/config.hpp>
#include <uboost_coro/context/fixedsize_stack.hpp>

#include <uboost_coro/coroutine2/detail/coroutine.hpp>

#ifdef BOOST_HAS_ABI_HEADERS
#  include BOOST_ABI_PREFIX
#endif

namespace boost {
namespace coroutines2 {

using fixedsize_stack = boost::context::fixedsize_stack;
#if !defined(BOOST_USE_SEGMENTED_STACKS)
using default_stack = boost::context::default_stack;
#endif

}}

#ifdef BOOST_HAS_ABI_HEADERS
#  include BOOST_ABI_SUFFIX
#endif

#endif // BOOST_COROUTINES2_FIXEDSIZE_H