////////////////////////////////////////////////////////////////////////////////
//         Distributed under the Boost Software License, Version 1.0.         //
//            (See accompanying file LICENSE or copy at                       //
//                 https://www.boost.org/LICENSE_1_0.txt)                     //
////////////////////////////////////////////////////////////////////////////////

#include "jobs/job_system_tests.h"

#include "jobs/fiber/fiber_job_system.h"

INSTANTIATE_TYPED_TEST_SUITE_P(fiber, JobSystemTests, iris::FiberJobSystem);
