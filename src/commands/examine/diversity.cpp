/*
    gappa - Genesis Applications for Phylogenetic Placement Analysis
    Copyright (C) 2017-2022 Pierre Barbera, Lucas Czech and HITS gGmbH

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

#include "commands/examine/diversity.hpp"

#include "options/global.hpp"
#include "tools/cli_setup.hpp"

#include "CLI/CLI.hpp"

#include "genesis/placement/function/diversity.hpp"

void setup_diversity( CLI::App& app )
{
    // Create the options and subcommand objects.
    auto opt = std::make_shared<DiversityOptions>();
    auto sub = app.add_subcommand(
        "diversity",
        "Calcualte various diversity metrics for a given set of samples."
    );

    // File input
    opt->jplace_input.add_jplace_input_opt_to_app( sub );

    // ==== which metrics should we calculate? ====
    // PD
    options->calculate_PD = sub->add_flag(
        "--PD",
        options->calculate_PD.value(),
        "Calculate Faith's Phylogenetic Diversity (PD) metric."
    );
    options->calculate_PD.option()->group( "Metrics" );

    // BWPD
    options->calculate_BWPD = sub->add_flag(
        "--BWPD",
        options->calculate_BWPD.value(),
        "Calculate the Balance Weighted Phylogenetic Diversity (BWPD) metric."
    );
    options->calculate_BWPD.option()->group( "Metrics" );

    // MPD
    options->calculate_MPD = sub->add_flag(
        "--MPD",
        options->calculate_MPD.value(),
        "Calculate the Mean Pairwise Distance (MPD) diversity metric."
    );
    options->calculate_MPD.option()->group( "Metrics" );

    // Output
    opt->file_output.add_default_output_opts_to_app( sub );

    // Set the run function as callback to be called when this subcommand is issued.
    // Hand over the options by copy, so that their shared ptr stays alive in the lambda.
    sub->callback( gappa_cli_callback(
        sub,
        {},
        [ opt ]() {
            run_diversity( *opt );
        }
    ));
}

// =================================================================================================
//      Run
// =================================================================================================

void run_diversity( DiversityOptions const& options )
{
    options.jplace_input.print();
    auto sample = options.jplace_input.merged_samples();

    if( options.calculate_PD ) {
        std::cout << "PD:\t" << PD( sample ) << std::endl;
    }
    if( options.calculate_BWPD ) {
        std::cout << "BWPD:\t" << BWPD( sample, 1.0 ) << std::endl;
    }
    if( options.calculate_MPD ) {
        // std::cout << "MPD:\t" << MPD( sample ) << std::endl;
    }
}