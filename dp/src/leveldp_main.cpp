// leveldp -- the command-line front end of the reachability-DP solver.
//
// The whole of it lives in dp/cli.hpp so that the Geode mod can run the identical code
// path in-process; it is compiled once, in dpcore.cpp (library gdsolver_dpcore), which the mod
// links too. This file exists to give the executable an entry point.
#include "dpcore.hpp"

int main(int argc, char** argv) {
    return dpcore::cliMain(argc, argv);
}
