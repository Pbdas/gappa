/*
    gappa - Genesis Applications for Phylogenetic Placement Analysis
    Copyright (C) 2017-2022 Lucas Czech

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
    Lucas Czech <lczech@carnegiescience.edu>
    Department of Plant Biology, Carnegie Institution For Science
    260 Panama Street, Stanford, CA 94305, USA
*/

#include "commands/examine/heat_tree.hpp"

#include "options/global.hpp"
#include "tools/cli_setup.hpp"

#include "CLI/CLI.hpp"

#include "genesis/placement/formats/jplace_reader.hpp"
#include "genesis/placement/function/helper.hpp"
#include "genesis/placement/function/masses.hpp"
#include "genesis/placement/function/operators.hpp"
#include "genesis/placement/function/tree.hpp"
#include "genesis/tree/common_tree/newick_writer.hpp"
#include "genesis/utils/core/fs.hpp"

#include <algorithm>
#include <stdexcept>

#ifdef GENESIS_OPENMP
#   include <omp.h>
#endif

// =================================================================================================
//      Setup
// =================================================================================================

void setup_heat_tree( CLI::App& app )
{
    // Create the options and subcommand objects.
    auto options = std::make_shared<HeatTreeOptions>();
    auto sub = app.add_subcommand(
        "heat-tree",
        "Make a tree with edges colored according to the placement mass of the samples."
    );

    // Input files.
    options->jplace_input.add_jplace_input_opt_to_app( sub );
    options->jplace_input.add_mass_norm_opt_to_app( sub, false );
    options->jplace_input.add_point_mass_opt_to_app( sub );
    options->jplace_input.add_ignore_multiplicities_opt_to_app( sub );

    // Color. We allow max, but not min, as this is always 0.
    options->color_map.add_color_list_opt_to_app( sub, "BuPuBk" );
    options->color_map.add_under_opt_to_app( sub );
    options->color_map.add_over_opt_to_app( sub );
    options->color_map.add_mask_opt_to_app( sub );
    options->color_norm.add_log_scaling_opt_to_app( sub );
    options->color_norm.add_min_value_opt_to_app( sub );
    options->color_norm.add_max_value_opt_to_app( sub );
    options->color_norm.add_mask_value_opt_to_app( sub );

    // Output files.
    options->file_output.add_default_output_opts_to_app( sub );
    options->tree_output.add_tree_output_opts_to_app( sub );

    // Set the run function as callback to be called when this subcommand is issued.
    // Hand over the options by copy, so that their shared ptr stays alive in the lambda.
    sub->callback( gappa_cli_callback(
        sub,
        {},
        [ options ]() {
            run_heat_tree( *options );
        }
    ));
}

// =================================================================================================
//      Run
// =================================================================================================

void run_heat_tree( HeatTreeOptions const& options )
{
    using namespace genesis;
    using namespace genesis::placement;
    using namespace genesis::tree;

    // Prepare output file names and check if any of them already exists. If so, fail early.
    std::vector<std::pair<std::string, std::string>> files_to_check;
    for( auto const& e : options.tree_output.get_extensions() ) {
        files_to_check.push_back({ "tree", e });
    }
    options.file_output.check_output_files_nonexistence( files_to_check );

    // User is warned when not using any tree outputs.
    options.tree_output.check_tree_formats();

    // User output.
    options.jplace_input.print();

    // Prepare results.
    Tree tree;
    std::vector<double> total_masses;
    size_t file_count = 0;

    // Read all jplace files and accumulate their masses.
    #pragma omp parallel for schedule(dynamic)
    for( size_t fi = 0; fi < options.jplace_input.file_count(); ++fi ) {

        // User output
        LOG_MSG2 << "Processing file " << ( ++file_count ) << " of " << options.jplace_input.file_count()
                 << ": " << options.jplace_input.file_path( fi );

        // Read in file. This also already applies all normalizations.
        auto const sample = options.jplace_input.sample( fi );

        // Get masses per edge.
        auto const masses = placement_mass_per_edges_with_multiplicities( sample );

        // The main accumulation is single threaded.
        // We could optimize more, but seriously, it is fast enough already.
        #pragma omp critical(GAPPA_VISUALIZE_ACCUMULATE)
        {
            // Tree
            if( tree.empty() ) {
                tree = sample.tree();
            } else if( ! genesis::placement::compatible_trees( tree, sample.tree() ) ) {
                throw std::runtime_error( "Input jplace files have differing reference trees." );
            }

            // Masses
            if( total_masses.empty() ) {
                total_masses = masses;
            } else if( total_masses.size() != masses.size() ) {
                throw std::runtime_error( "Input jplace files have differing reference trees." );
            } else {
                for( size_t i = 0; i < masses.size(); ++i ) {
                    total_masses[i] += masses[i];
                }
            }
        }
    }

    // If we use relative masses, we normalize the whole mass set once more, so that the sum is 1.
    if( options.jplace_input.mass_norm_relative() ) {
        auto const sum = std::accumulate( total_masses.begin(), total_masses.end(), 0.0 );
        for( size_t i = 0; i < total_masses.size(); ++i ) {
            total_masses[i] /= sum;
        }
    }

    // Get color map and norm.
    auto color_map  = options.color_map.color_map();
    auto color_norm = options.color_norm.get_sequential_norm();

    // First, autoscale to get the max.
    // Finally, apply the user settings that might have been provided.
    color_norm->autoscale( total_masses );
    auto const auto_min = color_norm->min_value();
    if( options.color_norm.log_scaling() ) {

        // Some user friendly safety. Min of 0 does not work with log scaling.
        // Instead, if we have a max > 1, we set min to 1, which is a good case for absolute abundances.
        // For relative abundances (normalized samples), the max is < 1, so we set the min to some
        // value below that that spans some orders of magnitude. This is all used as default anyway,
        // as users can overwrite this via --min-value.
        if( color_norm->min_value() <= 0.0 ) {
            if( color_norm->max_value() > 1.0 ) {
                color_norm->min_value( 1.0 );
            } else {
                color_norm->min_value( color_norm->max_value() / 10e4 );
            }
            // color_map.clip_under( true );
        }

    } else {
        color_norm->min_value( 0.0 );
    }

    // Now overwrite the above "default" settings with what the user specified
    // (in case that they actually did specify certain values).
    options.color_norm.apply_options( *color_norm );

    // Issue a warning if we needed to set the min due to log, but there was no manual overwrite,
    // and if this leads to having under values.
    if( options.color_norm.log_scaling() && auto_min <= 0.0 ) {
        if( ! *options.color_norm.min_value_option && ! *options.color_map.clip_under_option ) {
            LOG_WARN << "Warning: Some branches have mass 0, which cannot be shown using --log-scaling. "
                     << "Hence, the minimum was set to " << color_norm->min_value() << " instead.\n"
                     << "This will lead to those branches being shown in the color specified by "
                     << "--mask-color. Use --clip-under and --min-value to change this.";
        } else {

            // The log color norm yields -inf for 0 values.
            // But if we have clip under or a min value, this is not what we want.
            // So, set 0 values to something that is not invalid.
            for( auto& v : total_masses ) {
                if( v <= 0.0 ) {
                    v = color_norm->min_value() / 2.0;
                }
            }
        }
    }

    // Now, make a color vector and write to files.
    auto const colors = color_map( *color_norm, total_masses );
    options.tree_output.write_tree_to_files(
        tree,
        colors,
        color_map,
        *color_norm,
        options.file_output,
        "tree"
    );
}
