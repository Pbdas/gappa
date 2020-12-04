#ifndef GAPPA_COMMANDS_PREPARE_PHAT_H_
#define GAPPA_COMMANDS_PREPARE_PHAT_H_

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

#include "CLI/CLI.hpp"

#include "options/sequence_input.hpp"
#include "options/file_output.hpp"

#include <string>
#include <vector>

// =================================================================================================
//      Options
// =================================================================================================

class PhatOptions
{
public:

    // Input data.
    std::string taxonomy_file;
    std::string sequence_file;
    std::string sub_taxopath;

    // Entropy pruning options.
    size_t target_taxonomy_size = 0;
    size_t min_subclade_size    = 0;
    size_t max_subclade_size    = 0;
    size_t min_tax_level        = 0;
    bool   allow_approximation  = false;
    bool   no_taxa_selection    = false;

    // Consensus options.
    std::string consensus_method = "majorities";
    double consensus_threshold = 0.5;

    // Output options.
    FileOutputOptions file_output;
    bool write_info_files = false;

    // SequenceInputOptions sequence_input;
    // FileOutputOptions abundance_output;

};

// =================================================================================================
//      Functions
// =================================================================================================

void setup_phat( CLI::App& app );
void run_phat( PhatOptions const& options );

#endif // include guard
