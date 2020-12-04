/*
    gappa - Genesis Applications for Phylogenetic Placement Analysis
    Copyright (C) 2017-2020 Lucas Czech and HITS gGmbH

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

    Contact:
    Lucas Czech <lucas.czech@h-its.org>
    Exelixis Lab, Heidelberg Institute for Theoretical Studies
    Schloss-Wolfsbrunnenweg 35, D-69118 Heidelberg, Germany
*/

#include "commands/analyze/nhd.hpp"

#include "options/global.hpp"
#include "tools/cli_setup.hpp"

#include "CLI/CLI.hpp"

#include "genesis/placement/function/functions.hpp"
#include "genesis/placement/function/nhd.hpp"
#include "genesis/placement/function/operators.hpp"
#include "genesis/tree/common_tree/distances.hpp"
#include "genesis/tree/function/distances.hpp"
#include "genesis/tree/function/functions.hpp"
#include "genesis/utils/containers/matrix.hpp"
#include "genesis/utils/containers/matrix/operators.hpp"
#include "genesis/utils/io/output_stream.hpp"

#include <fstream>

#ifdef GENESIS_OPENMP
#   include <omp.h>
#endif

// =================================================================================================
//      Setup
// =================================================================================================

void setup_nhd( CLI::App& app )
{
    // Create the options and subcommand objects.
    auto opt = std::make_shared<NhdOptions>();
    auto sub = app.add_subcommand(
        "nhd",
        "Calcualte the pairwise Node Histogram Distance between samples."
    );

    // Add common options.
    opt->jplace_input.add_jplace_input_opt_to_app( sub );
    opt->jplace_input.add_point_mass_opt_to_app( sub );

    // Output
    std::string const matrix_optname = "";
    std::string const matrix_group = "Matrix Output";
    opt->file_output.set_optionname( matrix_optname );
    opt->file_output.set_group( matrix_group );
    opt->file_output.add_default_output_opts_to_app( sub );
    opt->file_output.add_file_compress_opt_to_app( sub );
    opt->matrix_output.add_matrix_output_opts_to_app( sub, "nhd" );

    // Add custom options.
    sub->add_option(
        "--histogram-bins",
        opt->bins,
        "Set how many bins are used per node histogram to represent the placement masses.",
        true
    );

    // Set the run function as callback to be called when this subcommand is issued.
    // Hand over the options by copy, so that their shared ptr stays alive in the lambda.
    sub->callback( gappa_cli_callback(
        sub,
        {},
        [ opt ]() {
            run_nhd( *opt );
        }
    ));
}

// =================================================================================================
//      Run
// =================================================================================================

void run_nhd( NhdOptions const& options )
{
    using namespace genesis;
    using namespace genesis::placement;
    using namespace genesis::tree;
    using namespace genesis::utils;

    // Check if any of the files we are going to produce already exists. If so, fail early.
    std::string const infix = "nhd_matrix";
    options.file_output.check_output_files_nonexistence( infix, "csv" );

    // Print some user output.
    options.jplace_input.print();
    LOG_MSG1 << "Reading samples and preparing node histograms.";

    // Prepare storage.
    auto const set_size = options.jplace_input.file_count();
    Matrix<double> node_distances;
    Matrix<signed char> node_sides;
    auto hist_vecs = std::vector<NodeDistanceHistogramSet>( set_size );
    size_t file_count = 0;

    // TODO do we need tree compatibility and size checks? this is implicitly covered by
    // the exceptions when calculating histogram distnaces, but we might want nicer errors.

    // Load files.
    #pragma omp parallel for schedule(dynamic)
    for( size_t fi = 0; fi < set_size; ++fi ) {

        // User output.
        LOG_MSG2 << "Processing file " << (++file_count) << " of " << set_size
                 << ": " << options.jplace_input.file_path( fi );

        // Read in file.
        auto const sample = options.jplace_input.sample( fi );

        // Calcualte matrices on first use. Whoever gets here first, calcualtes them.
        // The other threads wait for this to happen, and then skip the calcualtion.
        #pragma omp critical(GAPPA_NHD_CALCULATE_MATRICES)
        {
            if( node_distances.empty() ) {
                node_distances = node_branch_length_distance_matrix( sample.tree() );
                node_sides = node_root_direction_matrix( sample.tree() );
            }
        }

        // Fill the histograms for this sample.
        hist_vecs[fi] = node_distance_histogram_set( sample, node_distances, node_sides, options.bins );
    }

    LOG_MSG1 << "Calculating pairwise node histogram distances.";

    // Calcualte result matrix.
    auto const nhd_matrix = node_histogram_distance( hist_vecs );

    LOG_MSG1 << "Writing distance matrix.";
    options.matrix_output.write_matrix(
        options.file_output.get_output_target( infix, "csv" ),
        nhd_matrix
    );
}
