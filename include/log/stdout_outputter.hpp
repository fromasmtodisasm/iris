#pragma once

#include <string>

#include "log/outputter.hpp"

namespace eng
{

/**
 * Implementation of Outputter which writes log messages to stdout.
 */
class StdoutFormatter : public Outputter
{
    public:

        /** Default */
        ~StdoutFormatter() override = default;

        /**
         * Output log.
         *
         * @param log
         *   Log message to output.
         */
        void output(const std::string &log) override;
};

}

